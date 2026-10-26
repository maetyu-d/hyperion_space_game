#pragma once
#include "screen_layout.hpp"
namespace hyperion {
// Logical window coordinates shared by painting, clipping and pointer input.
struct UILayout {
 static constexpr float header=60,panelWidth=396,contentTop=112,footer=112;
 static float safeInset(float width,float height){return std::clamp(std::min(width,height)*.04f,24.f,40.f);}
 static float cockpitTop(float width,float height){return height-safeInset(width,height)-148;}
 static float panelLeft(float width){return width-panelWidth;}
 static float contentBottom(float height){return height-58;}
 static float worldRight(float width,bool panel){return panel?panelLeft(width):width;}
};
}
