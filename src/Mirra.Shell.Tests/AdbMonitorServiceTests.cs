using FluentAssertions;
using Mirra.Shell.Models;
using Mirra.Shell.Services;

namespace Mirra.Shell.Tests;

public sealed class AdbMonitorServiceTests
{
    [Fact]
    public void ParseDeviceList_SingleReadyDevice_ReturnsCorrectDevice()
    {
        const string raw =
            "List of devices attached\n" +
            "R3CN901ABCD\tdevice product:cheetah model:Pixel_7_Pro transport_id:1\n";

        var devices = AdbMonitorService.ParseDeviceList(raw);

        devices.Should().HaveCount(1);
        devices[0].Serial.Should().Be("R3CN901ABCD");
        devices[0].State.Should().Be(DeviceConnectionState.Ready);
        devices[0].Model.Should().Be("Pixel_7_Pro");
    }

    [Fact]
    public void ParseDeviceList_UnauthorizedDevice_ReturnsUnauthorizedState()
    {
        const string raw =
            "List of devices attached\n" +
            "emulator-5554\tunauthorized usb:1-2 transport_id:2\n";

        var devices = AdbMonitorService.ParseDeviceList(raw);

        devices.Should().HaveCount(1);
        devices[0].State.Should().Be(DeviceConnectionState.Unauthorized);
    }

    [Fact]
    public void ParseDeviceList_OfflineDevice_ReturnsOfflineState()
    {
        const string raw =
            "List of devices attached\n" +
            "ZY224XXXXX\toffline\n";

        var devices = AdbMonitorService.ParseDeviceList(raw);

        devices.Should().HaveCount(1);
        devices[0].State.Should().Be(DeviceConnectionState.Offline);
    }

    [Fact]
    public void ParseDeviceList_MultipleDevices_ReturnAll()
    {
        const string raw =
            "List of devices attached\n" +
            "ABC123\tdevice model:Pixel_6\n" +
            "DEF456\tunauthorized\n" +
            "GHI789\toffline\n";

        var devices = AdbMonitorService.ParseDeviceList(raw);
        devices.Should().HaveCount(3);
    }

    [Fact]
    public void ParseDeviceList_EmptyOutput_ReturnsEmptyList()
    {
        const string raw = "List of devices attached\n";
        var devices = AdbMonitorService.ParseDeviceList(raw);
        devices.Should().BeEmpty();
    }

    [Fact]
    public void ParseDeviceList_DeviceWithoutModel_ReturnsEmptyModel()
    {
        const string raw =
            "List of devices attached\n" +
            "ABC123\tdevice transport_id:1\n";

        var devices = AdbMonitorService.ParseDeviceList(raw);
        devices[0].Model.Should().BeEmpty();
        devices[0].DisplayName.Should().Be("ABC123");
    }

    [Fact]
    public void DeviceInfo_DisplayName_ReplacesUnderscoresWithSpaces()
    {
        var device = new DeviceInfo { Serial = "XYZ", Model = "Pixel_7_Pro" };
        device.DisplayName.Should().Be("Pixel 7 Pro");
    }
}
