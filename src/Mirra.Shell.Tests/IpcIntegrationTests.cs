using System.IO.Pipes;
using System.Text;
using System.Text.Json.Nodes;
using FluentAssertions;
using Microsoft.Extensions.Logging.Abstractions;
using Mirra.Shell.Services;

namespace Mirra.Shell.Tests;

public sealed class IpcIntegrationTests
{
    [Fact]
    public async Task IpcClientService_CanSendAndReceiveMessages()
    {
        string pipeName = $"mirra-test-pipe-{Guid.NewGuid()}";

        // Mock the C++ PipeServer
        using var mockServer = new NamedPipeServerStream(
            pipeName, PipeDirection.InOut, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous);

        var logger = NullLogger<IpcClientService>.Instance;
        using var client = new IpcClientService(logger);

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));

        // Start client connect in background
        var connectTask = client.ConnectAsync(pipeName, cts.Token);

        // Accept connection on the mock server
        await mockServer.WaitForConnectionAsync(cts.Token);
        await connectTask;

        // 1. Test Client -> Server (Send JSON from WPF to C++)
        var outgoingMsg = new { type = "client_to_server", payload = new { val = 42 } };
        await client.SendAsync(outgoingMsg, cts.Token);

        // Server reads message from pipe (simulating C++ framer)
        var lengthBuffer = new byte[4];
        await mockServer.ReadExactlyAsync(lengthBuffer, 0, 4, cts.Token);
        uint msgLen = BitConverter.ToUInt32(lengthBuffer, 0);
        
        var msgBuffer = new byte[msgLen];
        await mockServer.ReadExactlyAsync(msgBuffer, 0, (int)msgLen, cts.Token);
        string serverReceivedJson = Encoding.UTF8.GetString(msgBuffer);

        var serverReceivedObj = JsonNode.Parse(serverReceivedJson)!;
        serverReceivedObj["type"]!.ToString().Should().Be("client_to_server");
        serverReceivedObj["payload"]!["val"]!.GetValue<int>().Should().Be(42);

        // 2. Test Server -> Client (Send JSON from C++ to WPF)
        string jsonToClient = """{"type": "server_to_client", "payload": {"status": "ok"}}""";
        byte[] jsonBytes = Encoding.UTF8.GetBytes(jsonToClient);
        byte[] lenBytes = BitConverter.GetBytes((uint)jsonBytes.Length);
        
        await mockServer.WriteAsync(lenBytes, cts.Token);
        await mockServer.WriteAsync(jsonBytes, cts.Token);
        await mockServer.FlushAsync(cts.Token);

        // Client reads message
        var clientReceived = await client.Messages.ReadAsync(cts.Token);
        clientReceived["type"]!.ToString().Should().Be("server_to_client");
        clientReceived["payload"]!["status"]!.ToString().Should().Be("ok");
    }
}
