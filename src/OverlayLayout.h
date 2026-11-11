#pragma once
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

namespace dg::overlay::layout {
class StyleScope {
    int colors_=0;
public:
    StyleScope(){
        const float scale=ImGui::GetFontSize()/13.f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(14*scale,12*scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(8*scale,5*scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(10*scale,8*scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing,ImVec2(6*scale,4*scale));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,18*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,3*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        // Keep the scrollbar legible without consuming a large-font narrow
        // window's whole value/reset area.
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,16.f);
        const std::pair<ImGuiCol,ImVec4> colors[]={
            {ImGuiCol_Text,{.94f,.96f,.99f,1}},
            {ImGuiCol_TextDisabled,{.66f,.72f,.80f,1}},
            {ImGuiCol_WindowBg,{.055f,.075f,.105f,.98f}},
            {ImGuiCol_PopupBg,{.07f,.095f,.13f,.99f}},
            {ImGuiCol_Border,{.24f,.32f,.43f,1}},
            {ImGuiCol_FrameBg,{.11f,.16f,.22f,1}},
            {ImGuiCol_FrameBgHovered,{.17f,.26f,.36f,1}},
            {ImGuiCol_FrameBgActive,{.19f,.31f,.44f,1}},
            {ImGuiCol_Header,{.14f,.23f,.33f,1}},
            {ImGuiCol_HeaderHovered,{.20f,.35f,.49f,1}},
            {ImGuiCol_HeaderActive,{.22f,.39f,.55f,1}},
            {ImGuiCol_Button,{.16f,.30f,.44f,1}},
            {ImGuiCol_ButtonHovered,{.22f,.42f,.60f,1}},
            {ImGuiCol_ButtonActive,{.16f,.35f,.53f,1}},
            {ImGuiCol_CheckMark,{.43f,.84f,1.f,1}},
            {ImGuiCol_SliderGrab,{.36f,.70f,.93f,1}},
            {ImGuiCol_SliderGrabActive,{.55f,.87f,1.f,1}},
            {ImGuiCol_Separator,{.24f,.36f,.48f,1}}
        };
        for(const auto& color:colors){ImGui::PushStyleColor(color.first,color.second);++colors_;}
    }
    ~StyleScope(){ImGui::PopStyleColor(colors_);ImGui::PopStyleVar(9);}
    StyleScope(const StyleScope&)=delete;StyleScope& operator=(const StyleScope&)=delete;
};
inline bool Group(const char* title){
    ImGui::Spacing();const float scale=ImGui::GetFontSize()/13.f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(9*scale,7*scale));
    const bool open=ImGui::CollapsingHeader(title); // Retain IDs and closed-by-default policy.
    ImGui::PopStyleVar();return open;
}
inline float ValueIndent(){return ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x;}
inline float ControlWidth(float maximum=360.f){
    return std::max(1.f,std::min(ImGui::GetContentRegionAvail().x,maximum*ImGui::GetFontSize()/13.f));
}
inline void Label(const char* label){
    const auto text=std::string_view(label);const auto end=text.find("##");
    ImGui::TextWrapped("%.*s",int(end==text.npos?text.size():end),label);
}
// Keep one sentence visible. Decimal points are not sentence boundaries.
// The complete original explanation remains available on hover.
inline std::string_view Summary(std::string_view text){
    constexpr size_t limit=100;
    for(size_t i=0;i<std::min(text.size(),limit);++i){
        if((text[i]=='.'||text[i]=='!'||text[i]=='?')&&
           (i+1==text.size()||std::isspace(static_cast<unsigned char>(text[i+1]))))return text.substr(0,i+1);
    }
    if(text.size()<=limit)return text;
    size_t end=text.rfind(' ',limit);if(end==text.npos||end<40)end=limit;
    return text.substr(0,end);
}
inline void HelpText(const char* text){
    if(!text||!*text)return;
    const std::string_view full(text);const auto brief=Summary(full);const bool details=brief.size()<full.size();
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%.*s%s",int(brief.size()),text,details?"  (?)":"");
    ImGui::PopStyleColor();
    if(details&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)){
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*30.f);
        ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
}
inline void SameLineIfFits(float width){
    const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
    if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=right)ImGui::SameLine();
}
inline bool Button(const char* label,const ImVec2& size=ImVec2(0,0)){
    if(size.x<0)return ImGui::Button(label,size); // Fill an existing table cell.
    const float scale=ImGui::GetFontSize()/13.f;
    const float width=std::max(size.x*scale,ImGui::CalcTextSize(label,nullptr,true).x+2*ImGui::GetStyle().FramePadding.x);
    if(width>ImGui::GetContentRegionAvail().x)ImGui::NewLine();
    return ImGui::Button(label,ImVec2(width,size.y>0?size.y*scale:size.y));
}
template<class... Args> bool Choice(const char* label,int* value,Args&&... args){
    ImGui::PushID(label);Label(label);ImGui::SetNextItemWidth(ControlWidth());
    const bool changed=ImGui::Combo("##Choice",value,std::forward<Args>(args)...);
    ImGui::PopID();return changed;
}
inline bool NumberRow(const char* label,const char* id,float* value,
    float minimum,float maximum,float original,const char* format,const char* inputFormat="%.2f"){
    ImGui::PushID(id);Label(label);
    const float available=std::max(1.f,ImGui::GetContentRegionAvail().x);
    const float scale=ImGui::GetFontSize()/13.f,gap=ImGui::GetStyle().ItemSpacing.x;
    const float resetText=ImGui::CalcTextSize("Default").x;
    const float resetWidth=std::min(available,resetText+2*ImGui::GetStyle().FramePadding.x);
    const float manualWidth=std::min(100.f*scale,available);
    const bool oneLine=available>=160.f*scale+manualWidth+resetWidth+2*gap;
    ImGui::SetNextItemWidth(oneLine?available-manualWidth-resetWidth-2*gap:available);
    bool changed=ImGui::SliderFloat("##Slider",value,minimum,maximum,format);
    if(oneLine)ImGui::SameLine();
    ImGui::SetNextItemWidth(manualWidth);changed|=ImGui::InputFloat("##Manual",value,0,0,inputFormat);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Enter an exact value.");
    SameLineIfFits(resetWidth);
    // At the largest font in a narrow window, reduce padding before letting
    // the reset control overflow. Keep its full label, ID and click behavior.
    const auto padding=ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(std::max(0.f,std::min(padding.x,(resetWidth-resetText)*.5f)),padding.y));
    if(ImGui::Button("Default",ImVec2(resetWidth,0))){*value=original;changed=true;}
    ImGui::PopStyleVar();
    if(changed){if(!std::isfinite(*value))*value=original;*value=std::clamp(*value,minimum,maximum);}
    ImGui::PopID();return changed;
}
}
