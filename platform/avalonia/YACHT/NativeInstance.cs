using System.IO.Pipes;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
namespace YACHT;

// A separate channel per user/data directory also isolates the internal host and tests.
internal sealed class NativeInstance : IDisposable
{
    private readonly Mutex mutex;
    private readonly CancellationTokenSource stopped = new();
    private readonly string channel = "YACHT-Avalonia-" + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(Environment.UserName + AppIdentity.DataDirectory)))[..24];
    public bool IsPrimary { get; }
    public NativeInstance() { mutex = new Mutex(true, channel, out bool primary); IsPrimary = primary; }
    public async Task Forward(string[] paths)
    {
        byte[] bytes = JsonSerializer.SerializeToUtf8Bytes(paths); if (bytes.Length > 131072) throw new IOException("Too many files in activation request.");
        using var pipe = new NamedPipeClientStream(".", channel, PipeDirection.Out, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        await pipe.ConnectAsync(timeout.Token); await pipe.WriteAsync(BitConverter.GetBytes(bytes.Length), timeout.Token); await pipe.WriteAsync(bytes, timeout.Token);
    }
    public async Task Listen(Action<string[]> receive)
    {
        while (!stopped.IsCancellationRequested)
        {
            try
            {
                using var pipe = new NamedPipeServerStream(channel, PipeDirection.In, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
                await pipe.WaitForConnectionAsync(stopped.Token);
                using var timeout = CancellationTokenSource.CreateLinkedTokenSource(stopped.Token); timeout.CancelAfter(TimeSpan.FromSeconds(5));
                var header = new byte[4]; await pipe.ReadExactlyAsync(header, timeout.Token); int size = BitConverter.ToInt32(header);
                if (size < 0 || size > 131072) continue;
                var bytes = new byte[size]; await pipe.ReadExactlyAsync(bytes, timeout.Token);
                var paths = JsonSerializer.Deserialize<string[]>(bytes) ?? [];
                if (paths.All(p => p is not null && !p.StartsWith("--"))) receive(paths);
            }
            catch (OperationCanceledException) { }
            catch (IOException) { }
            catch (JsonException) { }
        }
    }
    public void Dispose() { stopped.Cancel(); mutex.Dispose(); }
}
