
#ifndef mozilla_dom_GamepadServiceType_h_
#define mozilla_dom_GamepadServiceType_h_

namespace mozilla{
namespace dom{

// Standard channel is used for managing gamepads that
// are from GamepadPlatformService.
enum class GamepadServiceType : uint16_t {
  Standard,
  NumGamepadServiceType
};

}// namespace dom
}// namespace mozilla

#endif // mozilla_dom_GamepadServiceType_h_