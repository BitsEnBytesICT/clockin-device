#pragma once

namespace clockin {

enum class UiControl {
    None,
    Admin,
    AttendanceBack,
    AttendanceConfirm,
    SignatureClear,
    SignatureCancel,
    SignatureSubmit,
    AdminBack,
    AdminTabRfid,
    AdminTabPin,
    AdminTabNetwork,
    AdminPinChange,
    AdminPinCancel,
    AdminPinRestart,
    AdminNetworkRefresh,
    AdminNetworkSelect,
    AdminNetworkDetailsBack,
    AdminNetworkConnect,
    AdminNetworkDisconnect,
    AdminNetworkForget,
    AdminNetworkConfirmForget,
    AdminNetworkCancel,
    AdminKeyboardKey,
    AdminKeyboardShift,
    AdminKeyboardSymbols,
    AdminKeyboardSpace,
    AdminKeyboardBackspace,
    AdminKeyboardVisibility,
    AdminKeyboardCancel,
    AdminKeyboardConnect,
    Key0,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
    Key6,
    Key7,
    Key8,
    Key9,
    KeyBackspace
};

}  // namespace clockin
