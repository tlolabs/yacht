using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace YACHT;

public sealed class CoreException(string code, string message) : Exception(message) { public string Code { get; } = code; }
public static class Core
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool CancelCallback(IntPtr context);
    [DllImport("yacht_ffi", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr yacht_request([MarshalAs(UnmanagedType.LPUTF8Str)] string request, CancelCallback callback, IntPtr context);
    [DllImport("yacht_ffi", CallingConvention = CallingConvention.Cdecl)]
    private static extern void yacht_free(IntPtr pointer);
    static Core()
    {
        NativeLibrary.SetDllImportResolver(typeof(Core).Assembly, (name, assembly, path) => name == "yacht_ffi" ? NativeLibrary.Load(Path.Combine(AppContext.BaseDirectory, OperatingSystem.IsWindows() ? "yacht_ffi.dll" : OperatingSystem.IsMacOS() ? "libyacht_ffi.dylib" : "libyacht_ffi.so")) : IntPtr.Zero);
    }
    public static JsonNode? Call(string operation, object? arguments = null, CancellationToken token = default)
    {
        var request = arguments is null ? new JsonObject() : JsonSerializer.SerializeToNode(arguments)!.AsObject();
        request["op"] = operation; request["version"] = 1;
        CancelCallback callback = _ => token.IsCancellationRequested;
        var pointer = yacht_request(request.ToJsonString(), callback, IntPtr.Zero);
        if (pointer == IntPtr.Zero) throw new InvalidOperationException("The Rust core returned no response.");
        try
        {
            var response = JsonNode.Parse(Marshal.PtrToStringUTF8(pointer)!)!;
            if (response["error"] is JsonNode error)
            {
                var code = error["code"]!.GetValue<string>();
                if (code == "cancelled") throw new OperationCanceledException(token);
                throw new CoreException(code, error["message"]!.GetValue<string>());
            }
            return response["ok"]?.DeepClone();
        }
        finally { yacht_free(pointer); GC.KeepAlive(callback); }
    }
    public static JsonObject Style(string operation = "defaults") => Call(operation)!.AsObject();
    public static Table Read(string path, string delimiter, CancellationToken token = default) => new(Call("read", new { path, delimiter }, token)!.AsObject());
    public sealed class Table(JsonObject metadata) : IDisposable
    {
        private readonly object gate = new();
        private bool released;
        public JsonObject Metadata { get; } = metadata;
        public ulong Handle => Metadata["handle"]!.GetValue<ulong>();
        public JsonNode? Call(string op, object? args = null, CancellationToken token = default)
        {
            var request = args is null ? new JsonObject() : JsonSerializer.SerializeToNode(args)!.AsObject();
            request["handle"] = Handle;
            lock (gate)
            {
                ObjectDisposedException.ThrowIf(released, this);
                try { return Core.Call(op, request, token); } finally { GC.KeepAlive(this); }
            }
        }
        public void Dispose()
        {
            lock (gate) { if (released) return; Core.Call("release", new { handle = Handle }); released = true; }
            GC.SuppressFinalize(this);
        }
        ~Table() { try { Dispose(); } catch { /* process shutdown */ } }
    }
}
