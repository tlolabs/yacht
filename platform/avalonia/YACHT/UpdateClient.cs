using System.Diagnostics;
using System.Text.Json.Nodes;
namespace YACHT;

internal static class UpdateClient
{
    public static async Task<JsonObject> Run(string command, params string[] arguments)
    {
        if (AppIdentity.IsInternal) throw new InvalidOperationException("Production updater unavailable in the internal reference host.");
        var start = new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory, OperatingSystem.IsWindows() ? "yacht-update.exe" : "yacht-update"))
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        start.ArgumentList.Add(command);
        foreach (var argument in arguments) start.ArgumentList.Add(argument);
        using var process = Process.Start(start) ?? throw new IOException("Cannot start update checker.");
        var output = process.StandardOutput.ReadToEndAsync();
        var error = process.StandardError.ReadToEndAsync();
        using var timeout = new CancellationTokenSource(TimeSpan.FromMinutes(5));
        try { await process.WaitForExitAsync(timeout.Token); }
        catch (OperationCanceledException)
        {
            if (!process.HasExited) process.Kill(entireProcessTree: true);
            await process.WaitForExitAsync();
            throw new IOException("The update helper timed out. Retry the update check later.");
        }
        if (process.ExitCode != 0) throw new IOException(await error);
        return JsonNode.Parse(await output)!.AsObject();
    }
    public static async Task VerifyInstaller(JsonObject download)
    {
        // PowerShell uses Windows Authenticode trust validation, including chain
        // validity; require the compiled policy's publisher as well as the digest.
        // Values are passed in child environment entries, never interpolated code.
        const string script = "& { $ErrorActionPreference='Stop'; " +
            "$p=$env:TLO_UPDATE_PATH; $s=Get-AuthenticodeSignature -LiteralPath $p; " +
            "if($s.Status -ne 'Valid' -or $s.SignerCertificate.Subject -cne $env:TLO_UPDATE_PUBLISHER){throw 'Invalid Authenticode publisher'}; " +
            "if((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() -cne $env:TLO_UPDATE_SHA256){throw 'Installer changed after download'}; " +
            "$info=[System.Diagnostics.FileVersionInfo]::GetVersionInfo($p); " +
            "if($info.ProductName -cne 'YACHT'){throw 'Installer application mismatch'}; $v=$info.ProductVersion; " +
            "if($v -ne $env:TLO_UPDATE_VERSION -and $v -ne ($env:TLO_UPDATE_VERSION+'.0')){throw 'Installer version mismatch'} }";
        var start = new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"WindowsPowerShell\v1.0\powershell.exe"))
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        foreach (var argument in new[] { "-NoProfile", "-NonInteractive", "-Command", script }) start.ArgumentList.Add(argument);
        start.Environment["TLO_UPDATE_PATH"] = download["path"]!.GetValue<string>();
        start.Environment["TLO_UPDATE_PUBLISHER"] = download["publisher"]!.GetValue<string>();
        start.Environment["TLO_UPDATE_SHA256"] = download["sha256"]!.GetValue<string>();
        start.Environment["TLO_UPDATE_VERSION"] = download["version"]!.GetValue<string>();
        using var process = Process.Start(start) ?? throw new IOException("Cannot verify installer.");
        var output = process.StandardOutput.ReadToEndAsync(); var error = process.StandardError.ReadToEndAsync();
        using var timeout = new CancellationTokenSource(TimeSpan.FromMinutes(5));
        try { await process.WaitForExitAsync(timeout.Token); }
        catch (OperationCanceledException)
        {
            if (!process.HasExited) process.Kill(entireProcessTree: true);
            await process.WaitForExitAsync();
            throw new IOException("The update helper timed out. Retry the update check later.");
        }
        await output;
        if (process.ExitCode != 0) throw new IOException(await error);
    }
}
