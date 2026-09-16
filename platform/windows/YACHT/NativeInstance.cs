using System.IO.Pipes;
using System.Security.Principal;
using System.Text.Json;
namespace YACHT;

/// Routes Explorer's separate file-activation processes into the existing native
/// window. The current-user pipe carries paths only, never conversion behavior.
internal sealed class NativeInstance : IDisposable
{
    private readonly Mutex mutex;
    private readonly string channel;
    private readonly CancellationTokenSource stopped = new();
    public bool IsPrimary { get; }
    public NativeInstance()
    {
        channel = "YACHT-" + WindowsIdentity.GetCurrent().User!.Value;
        mutex = new Mutex(true, "Local\\" + channel, out bool primary);
        IsPrimary = primary;
    }
    public async Task Forward(string[] paths)
    {
        using var pipe = new NamedPipeClientStream(".", channel, PipeDirection.Out, PipeOptions.Asynchronous);
        await pipe.ConnectAsync(5000);
        using var writer = new StreamWriter(pipe) { AutoFlush = true };
        await writer.WriteLineAsync(JsonSerializer.Serialize(paths));
    }
    public async Task Listen(Action<string[]> receive)
    {
        while (!stopped.IsCancellationRequested)
        {
            try
            {
                using var pipe = new NamedPipeServerStream(channel, PipeDirection.In, 1,
                    PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
                await pipe.WaitForConnectionAsync(stopped.Token);
                using var reader = new StreamReader(pipe);
                var line = await reader.ReadLineAsync(stopped.Token);
                if (line is not null) receive(JsonSerializer.Deserialize<string[]>(line) ?? []);
            }
            catch (OperationCanceledException) { break; }
            catch (IOException) { /* A launching process may exit before connecting. */ }
            catch (JsonException) { /* Ignore a malformed activation message. */ }
        }
    }
    public void Dispose() { stopped.Cancel(); mutex.Dispose(); }
}
