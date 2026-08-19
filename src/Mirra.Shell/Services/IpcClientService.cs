using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Channels;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

/// <summary>
/// Connects to the Casting.Core named pipe and handles bidirectional JSON-framed messages.
/// Wire format: [4-byte LE uint32 length][UTF-8 JSON payload]
/// </summary>
public sealed class IpcClientService : IDisposable
{
    private readonly ILogger<IpcClientService> _logger;
    private readonly ClipboardMonitorService   _clipboardMonitor;
    private NamedPipeClientStream?             _pipe;
    private CancellationTokenSource?           _cts;
    private Task?                              _readTask;

    // Incoming message channel (unbounded; readers should drain quickly)
    private readonly Channel<JsonObject> _inbound =
        Channel.CreateUnbounded<JsonObject>(new UnboundedChannelOptions { SingleWriter = true });

    public ChannelReader<JsonObject> Messages => _inbound.Reader;

    public IpcClientService(ILogger<IpcClientService> logger, ClipboardMonitorService clipboardMonitor)
    {
        _logger = logger;
        _clipboardMonitor = clipboardMonitor;
        _clipboardMonitor.ClipboardChanged += OnClipboardChanged;
    }

    private async void OnClipboardChanged(object? sender, string text)
    {
        // TODO: check if Sync is enabled. For now we just send.
        try
        {
            await SendAsync(new { v = 1, type = "CMD_SEND_CLIPBOARD", payload = new { text } });
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to send clipboard text over IPC.");
        }
    }

    public async Task ConnectAsync(string pipeName, CancellationToken ct = default)
    {
        _pipe = new NamedPipeClientStream(".", pipeName, PipeDirection.InOut, PipeOptions.Asynchronous);

        _logger.LogInformation("Connecting to named pipe: {PipeName}", pipeName);
        await _pipe.ConnectAsync(10_000, ct);
        _logger.LogInformation("Connected to Casting.Core pipe.");

        _cts      = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _readTask = Task.Run(() => ReadLoopAsync(_cts.Token), _cts.Token);
    }

    public async Task SendAsync(object payload, CancellationToken ct = default)
    {
        if (_pipe is null || !_pipe.IsConnected) return;

        string json = JsonSerializer.Serialize(payload);
        byte[] jsonBytes = Encoding.UTF8.GetBytes(json);

        // 4-byte LE length prefix
        byte[] lenBytes = BitConverter.GetBytes((uint)jsonBytes.Length);
        if (!BitConverter.IsLittleEndian) Array.Reverse(lenBytes);

        var frame = new byte[4 + jsonBytes.Length];
        lenBytes.CopyTo(frame, 0);
        jsonBytes.CopyTo(frame, 4);

        await _pipe.WriteAsync(frame, ct);
        await _pipe.FlushAsync(ct);
    }

    private async Task ReadLoopAsync(CancellationToken ct)
    {
        var buffer = new byte[65536];

        // Running reassembly buffer
        var reassembly = new List<byte>(65536);

        try
        {
            while (!ct.IsCancellationRequested && _pipe!.IsConnected)
            {
                int bytesRead = await _pipe.ReadAsync(buffer, ct);
                if (bytesRead == 0) break;

                reassembly.AddRange(buffer[..bytesRead]);

                // Extract complete messages
                while (TryExtractMessage(reassembly, out string? json))
                {
                    try
                    {
                        var obj = JsonNode.Parse(json!)?.AsObject();
                        if (obj is not null)
                        {
                            string type = obj["type"]?.GetValue<string>() ?? string.Empty;
                            if (type == "CMD_SET_CLIPBOARD")
                            {
                                string text = obj["payload"]?["text"]?.GetValue<string>() ?? string.Empty;
                                _clipboardMonitor.SetClipboard(text);
                            }
                            else
                            {
                                await _inbound.Writer.WriteAsync(obj, ct);
                            }
                        }
                    }
                    catch (JsonException ex)
                    {
                        _logger.LogWarning(ex, "Failed to parse IPC message: {Json}", json);
                        // Per PRD: malformed message must fail safely — log and continue
                    }
                }
            }
        }
        catch (OperationCanceledException) { /* normal shutdown */ }
        catch (Exception ex)
        {
            _logger.LogError(ex, "IPC read loop error");
        }
        finally
        {
            _inbound.Writer.TryComplete();
            _logger.LogInformation("IPC read loop exited.");
        }
    }

    private static bool TryExtractMessage(List<byte> buf, out string? json)
    {
        json = null;
        if (buf.Count < 4) return false;

        uint msgLen =
            buf[0] |
            ((uint)buf[1] << 8)  |
            ((uint)buf[2] << 16) |
            ((uint)buf[3] << 24);

        if (msgLen > 16 * 1024 * 1024) throw new InvalidDataException("IPC message too large");
        if (buf.Count < 4 + msgLen)    return false;

        json = Encoding.UTF8.GetString(buf.ToArray(), 4, (int)msgLen);
        buf.RemoveRange(0, 4 + (int)msgLen);
        return true;
    }

    public void Dispose()
    {
        _clipboardMonitor.ClipboardChanged -= OnClipboardChanged;
        _cts?.Cancel();
        _readTask?.GetAwaiter().GetResult();
        _pipe?.Dispose();
        _cts?.Dispose();
    }
}
