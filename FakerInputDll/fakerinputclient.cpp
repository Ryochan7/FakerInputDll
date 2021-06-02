#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier

#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdlib.h>
#include <iostream>

#include "fakerinputclient.h"
#include "fakerinputcommon.h"

typedef struct _fakerinput_client_t
{
    HANDLE hControl;
    HANDLE hMethodEndpoint;
    BYTE controlReport[CONTROL_REPORT_SIZE];
} fakerinput_client_t;

//
// These are the device attributes returned by the mini driver in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//
#define FAKERINPUT_VID 0xFE0F
#define FAKERINPUT_PID 0x00FF
#define FAKERINPUT_VERSION 0x0101

//
// Function prototypes
//

HANDLE
SearchMatchingHwID(
    USAGE myUsagePage,
    USAGE myUsage
);

HANDLE
OpenDeviceInterface(
    HDEVINFO HardwareDeviceInfo,
    PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
    USAGE myUsagePage,
    USAGE myUsage
);

BOOLEAN
CheckIfOurDevice(
    HANDLE file,
    USAGE myUsagePage,
    USAGE myUsage
);

BOOL
HidOutput(
    BOOL useSetOutputReport,
    HANDLE file,
    PCHAR buffer,
    ULONG bufferSize
);


pfakerinput_client fakerinput_alloc()
{
    return (pfakerinput_client)malloc(sizeof(fakerinput_client_t));
}

void fakerinput_free(pfakerinput_client clientHandle)
{
    free(clientHandle);
}

bool fakerinput_connect(pfakerinput_client clientHandle)
{
    // Find HID device
    clientHandle->hControl = SearchMatchingHwID(0xff00, 0x0001);
    if (clientHandle->hControl == INVALID_HANDLE_VALUE ||
        clientHandle->hControl == nullptr)
        return false;

    clientHandle->hMethodEndpoint = SearchMatchingHwID(0xff00, 0x0002);
    if (clientHandle->hMethodEndpoint == INVALID_HANDLE_VALUE ||
        clientHandle->hMethodEndpoint == nullptr)
    {
        fakerinput_disconnect(clientHandle);
        return false;
    }

    BYTE testBuffer[65] = { 0 };
    ZeroMemory(testBuffer, CONTROL_REPORT_SIZE);
    //testBuffer[0] = REPORTID_API_VERSION_FEATURE_ID;
    /*FakerInputMethodReportHeader* methodReport = (FakerInputMethodReportHeader*)testBuffer;
    methodReport->ReportID = REPORTID_METHOD;
    methodReport->MethodEndpointID = FAKERINPUT_CHECK_API_VERSION;
    methodReport->ReportLength = sizeof(FakerInputAPIVersionReport);*/

    FakerInputAPIVersionReport* versionReport = (FakerInputAPIVersionReport*)(testBuffer);
    versionReport->ReportID = REPORTID_CHECK_API_VERSION;
    versionReport->ApiVersion = FAKERINPUT_API_VERSION;

    bool testStatus = HidOutput(FALSE, clientHandle->hMethodEndpoint, (PCHAR)testBuffer, CONTROL_REPORT_SIZE);
    //DWORD whyme = GetLastError();
    if (!testStatus)
    {
        fakerinput_disconnect(clientHandle);
        return false;
    }

    /*bool testStatus = HidD_GetFeature(clientHandle->hMethodEndpoint, (PCHAR)testBuffer, CONTROL_REPORT_SIZE);
    DWORD whyme = GetLastError();
    */

    // Set the buffer count to 10 on the control HID
    /*if (!HidD_SetNumInputBuffers(clientHandle->hControl, 10))
    {
        std::cout << "failed HidD_SetNumInputBuffers "
            << GetLastError() << std::endl;

        fakerinput_disconnect(clientHandle);
        return false;
    }*/

    return true;
}

void fakerinput_disconnect(pfakerinput_client clientHandle)
{
    if (clientHandle->hControl != nullptr)
    {
        CloseHandle(clientHandle->hControl);
        clientHandle->hControl = nullptr;
    }

    if (clientHandle->hMethodEndpoint != nullptr)
    {
        CloseHandle(clientHandle->hMethodEndpoint);
        clientHandle->hMethodEndpoint = nullptr;
    }
}

bool fakerinput_update_keyboard(pfakerinput_client clientHandle,
    BYTE shiftKeyFlags, BYTE keyCodes[KBD_KEY_CODES])
{
    FakerInputControlReportHeader* pReport = NULL;
    FakerInputKeyboardReport* pKeyboardReport = NULL;

    //
    // Set the report header
    //
    ZeroMemory(clientHandle->controlReport, CONTROL_REPORT_SIZE);
    pReport = (FakerInputControlReportHeader*)clientHandle->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(FakerInputKeyboardReport);

    //
    // Set the input report
    //

    pKeyboardReport = (FakerInputKeyboardReport*)(clientHandle->controlReport + sizeof(FakerInputControlReportHeader));
    pKeyboardReport->ReportID = REPORTID_KEYBOARD;
    pKeyboardReport->ShiftKeyFlags = shiftKeyFlags;
    RtlCopyMemory(pKeyboardReport->KeyCodes, keyCodes, KBD_KEY_CODES);
    //memcpy(pKeyboardReport->KeyCodes, keyCodes, KBD_KEY_CODES);

    // Send the report
    return HidOutput(FALSE, clientHandle->hControl, (PCHAR)clientHandle->controlReport, CONTROL_REPORT_SIZE);
}

bool fakerinput_update_keyboard_enhanced(pfakerinput_client clientHandle, BYTE multiKeys, BYTE extraKeys)
{
    FakerInputControlReportHeader* pReport = NULL;
    FakerInputMultimediaReport* pKeyboardReport = NULL;

    //
    // Set the report header
    //

    ZeroMemory(clientHandle->controlReport, CONTROL_REPORT_SIZE);
    pReport = (FakerInputControlReportHeader*)clientHandle->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(FakerInputMultimediaReport);

    //
    // Set the input report
    //

    pKeyboardReport = (FakerInputMultimediaReport*)(clientHandle->controlReport + sizeof(FakerInputControlReportHeader));
    pKeyboardReport->ReportID = REPORTID_ENHANCED_KEY;
    //RtlCopyMemory(&pKeyboardReport->MultimediaKeys0, &multiKeys, 1);
    pKeyboardReport->MultimediaKeys0 = multiKeys;
    pKeyboardReport->MultimediaKeys1 = extraKeys;
    pKeyboardReport->MultimediaKeys2 = (BYTE)0;

    // Send the report
    return HidOutput(FALSE, clientHandle->hControl, (PCHAR)clientHandle->controlReport, CONTROL_REPORT_SIZE);
}

bool fakerinput_update_absolute_mouse(pfakerinput_client clientHandle, BYTE button, USHORT x, USHORT y,
    BYTE wheelPosition, BYTE hWheelPosition)
{
    UNREFERENCED_PARAMETER(hWheelPosition);

    FakerInputControlReportHeader* pReport = NULL;
    FakerInputAbsMouseReport* pMouseReport = NULL;

    if (CONTROL_REPORT_SIZE <= sizeof(FakerInputControlReportHeader) + sizeof(FakerInputAbsMouseReport))
    {
        return FALSE;
    }

    ZeroMemory(clientHandle->controlReport, CONTROL_REPORT_SIZE);
    //
    // Set the report header
    //

    pReport = (FakerInputControlReportHeader*)clientHandle->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(FakerInputAbsMouseReport);

    //
    // Set the input report
    //

    pMouseReport = (FakerInputAbsMouseReport*)(clientHandle->controlReport + sizeof(FakerInputControlReportHeader));
    pMouseReport->ReportID = REPORTID_ABSOLUTE_MOUSE;
    pMouseReport->Button = button;
    pMouseReport->XValue = x;
    pMouseReport->YValue = y;
    pMouseReport->WheelPosition = wheelPosition;
    //pMouseReport->HWheelPosition = hWheelPosition;

    // Send the report
    return HidOutput(FALSE, clientHandle->hControl, (PCHAR)clientHandle->controlReport, CONTROL_REPORT_SIZE);
}

bool fakerinput_update_relative_mouse(pfakerinput_client clientHandle, BYTE button,
    SHORT x, SHORT y, BYTE wheelPosition, BYTE hWheelPosition)
{
    FakerInputControlReportHeader* pReport = NULL;
    FakerInputRelativeMouseReport* pMouseReport = NULL;

    if (CONTROL_REPORT_SIZE <= sizeof(FakerInputControlReportHeader) + sizeof(FakerInputRelativeMouseReport))
    {
        return FALSE;
    }

    ZeroMemory(clientHandle->controlReport, CONTROL_REPORT_SIZE);
    //
    // Set the report header
    //

    pReport = (FakerInputControlReportHeader*)clientHandle->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(FakerInputRelativeMouseReport);

    //
    // Set the input report
    //

    pMouseReport = (FakerInputRelativeMouseReport*)(clientHandle->controlReport + sizeof(FakerInputControlReportHeader));
    pMouseReport->ReportID = REPORTID_RELATIVE_MOUSE;
    pMouseReport->Button = button;
    pMouseReport->XValue = x;
    pMouseReport->YValue = y;
    pMouseReport->WheelPosition = wheelPosition;
    pMouseReport->HWheelPosition = hWheelPosition;

    // Send the report
    return HidOutput(FALSE, clientHandle->hControl, (PCHAR)clientHandle->controlReport, CONTROL_REPORT_SIZE);
}

HANDLE
SearchMatchingHwID(
    USAGE myUsagePage,
    USAGE myUsage
)
{
    HDEVINFO                  hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA  deviceInterfaceData;
    SP_DEVINFO_DATA           devInfoData;
    GUID                      hidguid;
    int                       i;

    HidD_GetHidGuid(&hidguid);

    hardwareDeviceInfo =
        SetupDiGetClassDevs((LPGUID)&hidguid,
            NULL,
            NULL, // Define no
            (DIGCF_PRESENT |
                DIGCF_INTERFACEDEVICE));

    if (INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    //
    // Enumerate devices of this interface class
    //

    printf("\n....looking for our HID device (with UP=0x%x "
        "and Usage=0x%x)\n", myUsagePage, myUsage);

    for (i = 0; SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,
        0, // No care about specific PDOs
        (LPGUID)&hidguid,
        i, //
        &deviceInterfaceData);
        i++)
    {

        //
        // Open the device interface and Check if it is our device
        // by matching the Usage page and Usage from Hid_Caps.
        // If this is our device then send the hid request.
        //

        HANDLE file = OpenDeviceInterface(hardwareDeviceInfo, &deviceInterfaceData, myUsagePage, myUsage);

        if (file != INVALID_HANDLE_VALUE)
        {
            SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
            return file;
        }

        //
        //device was not found so loop around.
        //

    }

    printf("Failure: Could not find our HID device \n");

    SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

    return INVALID_HANDLE_VALUE;
}

HANDLE
OpenDeviceInterface(
    HDEVINFO hardwareDeviceInfo,
    PSP_DEVICE_INTERFACE_DATA deviceInterfaceData,
    USAGE myUsagePage,
    USAGE myUsage
)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;

    DWORD        predictedLength = 0;
    DWORD        requiredLength = 0;
    HANDLE       file = INVALID_HANDLE_VALUE;

    SetupDiGetDeviceInterfaceDetail(
        hardwareDeviceInfo,
        deviceInterfaceData,
        NULL, // probing so no output buffer yet
        0, // probing so output buffer length of zero
        &requiredLength,
        NULL
    ); // not interested in the specific dev-node

    predictedLength = requiredLength;

    deviceInterfaceDetailData =
        (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(predictedLength);

    if (!deviceInterfaceDetailData)
    {
        printf("Error: OpenDeviceInterface: malloc failed\n");
        goto cleanup;
    }

    deviceInterfaceDetailData->cbSize =
        sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(
        hardwareDeviceInfo,
        deviceInterfaceData,
        deviceInterfaceDetailData,
        predictedLength,
        &requiredLength,
        NULL))
    {
        printf("Error: SetupDiGetInterfaceDeviceDetail failed\n");
        free(deviceInterfaceDetailData);
        goto cleanup;
    }

    file = CreateFile(deviceInterfaceDetailData->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_READ,
        NULL, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        0, // No special attributes
        NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
        printf("Error: CreateFile failed: %d\n", GetLastError());
        goto cleanup;
    }

    if (CheckIfOurDevice(file, myUsagePage, myUsage)) {

        goto cleanup;

    }

    CloseHandle(file);

    file = INVALID_HANDLE_VALUE;

cleanup:

    free(deviceInterfaceDetailData);

    return file;

}


BOOLEAN
CheckIfOurDevice(
    HANDLE file,
    USAGE myUsagePage,
    USAGE myUsage)
{
    PHIDP_PREPARSED_DATA Ppd = NULL; // The opaque parser info describing this device
    HIDD_ATTRIBUTES                 Attributes; // The Attributes of this hid device.
    HIDP_CAPS                       Caps; // The Capabilities of this hid device.
    BOOLEAN                         result = FALSE;

    if (!HidD_GetPreparsedData(file, &Ppd))
    {
        printf("Error: HidD_GetPreparsedData failed \n");
        goto cleanup;
    }

    if (!HidD_GetAttributes(file, &Attributes))
    {
        printf("Error: HidD_GetAttributes failed \n");
        goto cleanup;
    }

    if (Attributes.VendorID == FAKERINPUT_VID &&
        Attributes.ProductID == FAKERINPUT_PID)
    {
        if (!HidP_GetCaps(Ppd, &Caps))
        {
            printf("Error: HidP_GetCaps failed \n");
            goto cleanup;
        }

        if ((Caps.UsagePage == myUsagePage) && (Caps.Usage == myUsage))
        {
            printf("Success: Found my device.. \n");
            result = TRUE;
        }
    }

cleanup:

    if (Ppd != NULL)
    {
        HidD_FreePreparsedData(Ppd);
    }

    return result;
}

BOOL
HidOutput(
    BOOL useSetOutputReport,
    HANDLE file,
    PCHAR buffer,
    ULONG bufferSize
)
{
    ULONG bytesWritten;
    if (useSetOutputReport)
    {
        //
        // Send Hid report thru HidD_SetOutputReport API
        //

        if (!HidD_SetOutputReport(file, buffer, bufferSize))
        {
            printf("failed HidD_SetOutputReport %d\n", GetLastError());
            return FALSE;
        }
    }
    else
    {
        if (!WriteFile(file, buffer, bufferSize, &bytesWritten, NULL))
        {
            printf("failed WriteFile %d\n", GetLastError());
            return FALSE;
        }
    }

    return TRUE;
}
