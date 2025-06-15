#include "stdafx.h"

#include <Widgets/ExtraWeaponsets.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Item.h>

#include <Modules/Resources.h>
#include <Modules/GwDatTextureModule.h>
#include <Utils/TextUtils.h>

#include <Keys.h>

namespace 
{
    std::unordered_map<std::wstring, std::wstring> decodedNames;
    std::map<GW::Constants::Bag, std::vector<InventoryItem>> available_items;
    constexpr float weaponOffsetFactor = 0.075f;

    std::string removeTextInBrackets(std::string str)
    {
        while (true) {
            const auto left = str.find('<');
            if (left == std::string::npos) return str;
            const auto right = str.find('>', left);
            if (right == std::string::npos) return str;
            str.erase(left, right + 1);
        }
    }
    std::string decode(const std::wstring& wstring)
    {
        if (wstring.empty()) return "";
        auto& decoded = decodedNames[wstring];
        if (decoded.empty()) {
            GW::GameThread::Enqueue([wstring, &decoded] {
                GW::UI::AsyncDecodeStr(wstring.c_str(), &decoded);
            });
        }
        return removeTextInBrackets(TextUtils::WStringToString(decoded));
    }

    bool isWeapon(const GW::Item* item)
    {
        if (!item) {
            return false;
        }

        switch (static_cast<GW::Constants::ItemType>(item->type)) {
            case GW::Constants::ItemType::Axe:
            case GW::Constants::ItemType::Bow:
            case GW::Constants::ItemType::Offhand:
            case GW::Constants::ItemType::Hammer:
            case GW::Constants::ItemType::Wand:
            case GW::Constants::ItemType::Shield:
            case GW::Constants::ItemType::Staff:
            case GW::Constants::ItemType::Sword:
            case GW::Constants::ItemType::Daggers:
            case GW::Constants::ItemType::Scythe:
            case GW::Constants::ItemType::Spear:
                return true;
            default:
                return false;
        }
    }


    void forEachItem(std::function<bool(GW::Item*)> func)
    {
        using GW::Constants::Bag;
        for (auto bagSlot : {Bag::Backpack, Bag::Belt_Pouch, Bag::Bag_1, Bag::Bag_2, Bag::Equipment_Pack, Bag::Equipped_Items}) {
            const auto bag = GW::Items::GetBag(bagSlot);
            if (!bag) continue;

            const auto& items = bag->items;
            for (size_t slot = 0; slot < items.size(); slot++) {
                const auto& item = items[slot];
                if (isWeapon(item)) {
                    if (func(item)) return;
                }
            }
        }
    }
    
    bool compareMods(const std::vector<GW::ItemModifier>& vec, const GW::ItemModifier* ptr, size_t size)
    {
        if (vec.size() != size) return false;
        for (size_t i = 0; i < size; ++i)
            if (vec[i].mod != ptr[i].mod) return false;
        return true;
    }

    std::vector<GW::ItemModifier> extractMods(const GW::Item* item)
    {
        std::vector<GW::ItemModifier> result;
        result.resize(item->mod_struct_size);
        for (auto i = 0u; i < item->mod_struct_size; ++i) {
            result[i] = item->mod_struct[i];
        }
        return result;
    }

    void drawItemSelector(InventoryItem& inventoryItem)
    {
        static bool needToFetchBagItems = false;
        constexpr auto bags = std::array{"None", "Backpack", "Belt Pouch", "Bag 1", "Bag 2", "Equipment Pack"};

        if (ImGui::Button("Edit item")) {
            needToFetchBagItems = true;
            ImGui::OpenPopup("Choose item to adjust");
        }
        ImGui::SameLine();

        if (inventoryItem.modelID == 0) {
            ImGui::TextUnformatted("No item chosen");
            ImGui::SameLine();
        }
        else if (!inventoryItem.encodedName.empty()) {
            ImGui::TextUnformatted(decode(inventoryItem.encodedName).c_str());
            ImGui::SameLine();
        }
        else {
            // Encoded names are not serialized to avoid wstring headache (and maybe issues when the game updates). Find the name.
            forEachItem([&inventoryItem](const GW::Item* item) {
                if (item->model_id == inventoryItem.modelID && compareMods(inventoryItem.modifiers, item->mod_struct, item->mod_struct_size)) {
                    inventoryItem.encodedName = item->single_item_name;
                    return true;
                }
                return false;
            });
        }

        if (ImGui::BeginPopupModal("Choose item to adjust", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (needToFetchBagItems) {
                available_items.clear();
                forEachItem([&](const GW::Item* item) {
                    const auto bag = item->bag ? item->bag->bag_id() : GW::Constants::Bag::Backpack;
                    available_items[bag].push_back(InventoryItem{item->model_id, item->single_item_name, extractMods(item)});
                    return false;
                });
                needToFetchBagItems = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            for (auto& [bag, bagItems] : available_items) {
                ImGui::TextUnformatted(bag == GW::Constants::Bag::Equipped_Items ? "Equipped items" : bags[(uint32_t)bag]);
                ImGui::Indent();
                for (auto& bagItem : bagItems) {
                    ImGui::PushID(&bagItem);
                    if (ImGui::Button(decode(bagItem.encodedName).c_str())) {
                        inventoryItem = bagItem;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    std::string makeHotkeyDescription(Hotkey hotkey)
    {
        char newDescription[256];
        ModKeyName(newDescription, _countof(newDescription), hotkey.modifier, hotkey.keyData);
        return std::string{newDescription};
    }

    bool drawHotkeySelector(Hotkey& hotkey, std::string& description)
    {
        bool keyChanged = false;

        ImGui::PushItemWidth(100);
        if (ImGui::Button(description.c_str())) 
        {
            ImGui::OpenPopup("Select Hotkey");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
        if (ImGui::BeginPopup("Select Hotkey")) {
            static long newkey = 0;
            char newkey_buf[256]{};
            long newmod = 0;

            ImGui::Text("Press key");
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) newmod |= ModKey_Control;
            if (ImGui::IsKeyDown(ImGuiMod_Shift)) newmod |= ModKey_Shift;
            if (ImGui::IsKeyDown(ImGuiMod_Alt)) newmod |= ModKey_Alt;

            if (newkey == 0) { // we are looking for the key
                for (WORD i = 0; i < 512; i++) {
                    switch (i) {
                        case VK_CONTROL:
                        case VK_LCONTROL:
                        case VK_RCONTROL:
                        case VK_SHIFT:
                        case VK_LSHIFT:
                        case VK_RSHIFT:
                        case VK_MENU:
                        case VK_LMENU:
                        case VK_RMENU:
                        case VK_LBUTTON:
                        case VK_RBUTTON:
                            continue;
                        default: {
                            if (::GetKeyState(i) & 0x8000) newkey = i;
                        }
                    }
                }
            }
            else if (!(::GetKeyState(newkey) & 0x8000)) { // We have a key, check if it was released
                keyChanged = true;
                hotkey.keyData = newkey;
                hotkey.modifier = newmod;
                description = makeHotkeyDescription(hotkey);
                newkey = 0;
                ImGui::CloseCurrentPopup();
            }

            ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
            ImGui::Text("%s", newkey_buf);

            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();

        return keyChanged;
    }
} // namespace

void ExtraWeaponsetsWidget::Initialize()
{
    ToolboxWidget::Initialize();
}


void ExtraWeaponsetsWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    
}

void ExtraWeaponsetsWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
}

void ExtraWeaponsetsWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    ImGui::SliderInt("Size", &size, 1, 400);
    ImGui::SliderInt("Columns", &columns, 1, 16);
    ImGui::Text("Weapon sets:");


    for (auto& weaponSet : weaponSets) 
    {
        ImGui::PushID(&weaponSet);
        ImGui::Bullet();
        
        ImGui::PushID(0);
        ImGui::Text("Mainhand:");
        ImGui::SameLine();
        drawItemSelector(weaponSet.mainHand);
        ImGui::PopID();

        ImGui::PushID(1);
        ImGui::SameLine();
        ImGui::Text("Offhand:");
        ImGui::SameLine();
        drawItemSelector(weaponSet.offHand);
        ImGui::PopID();

        std::string hotkeyText = "Select hotkey";
        drawHotkeySelector(weaponSet.hotkey, hotkeyText);

        ImGui::PopID();
    }

    if (ImGui::Button("+")) 
    {
        weaponSets.push_back({{}, {}});
    }
}

bool ExtraWeaponsetsWidget::OnMouseDown(const UINT, const WPARAM, const LPARAM lParam)
{
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    Log::Info("Clicked (%i, %i)", x, y);
    // Todo: Switch active weaponset. see minimap.cpp
    return false;
}

bool ExtraWeaponsetsWidget::WndProc(const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    const auto isActive = Instance().visible && !GW::Chat::GetIsTyping() && weaponSets.size() > 0 && GW::Map::GetIsMapLoaded() && !GW::UI::GetIsWorldMapShowing() &&
                          GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetObservingId() != 0;
    if (!isActive) return false;
    
    Hotkey pressedKey{};
    
    switch (message) {
        case WM_LBUTTONDOWN:
            return OnMouseDown(message, wParam, lParam);
        case WM_KEYDOWN:
            if (const auto isRepeated = (int)lParam & (1 << 30)) break;
            [[fallthrough]];
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            pressedKey.keyData = static_cast<int>(wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            if (LOWORD(wParam) & MK_MBUTTON) {
                pressedKey.keyData = VK_MBUTTON;
            }
            if (LOWORD(wParam) & MK_XBUTTON1) {
                pressedKey.keyData = VK_XBUTTON1;
            }
            if (LOWORD(wParam) & MK_XBUTTON2) {
                pressedKey.keyData = VK_XBUTTON2;
            }
            break;
        case WM_MBUTTONDBLCLK:
            pressedKey.keyData = VK_MBUTTON;
            break;
        default:
            break;
    }

    if (!pressedKey.keyData) return false;
    if (GetKeyState(VK_CONTROL) < 0) pressedKey.modifier |= ModKey_Control;
    if (GetKeyState(VK_SHIFT) < 0) pressedKey.modifier |= ModKey_Shift;
    if (GetKeyState(VK_MENU) < 0) pressedKey.modifier |= ModKey_Alt;
    
    auto weaponSet = std::ranges::find_if(weaponSets, [&](const auto& set){return set.hotkey == pressedKey;});
    if (weaponSet == weaponSets.end()) return false;

    for (auto& set : weaponSets) set.isActive = false;
    weaponSet->isActive = true;

    const auto& items = GW::Items::GetItemArray();
    if (!items) return false;

    const auto mainHand = std::ranges::find_if(*items, [&](const auto* item) { return weaponSet->mainHand.modelID && item && item->model_id == weaponSet->mainHand.modelID;});
    const auto offHand = std::ranges::find_if(*items, [&](const auto* item) { return weaponSet->offHand.modelID && item && item->model_id == weaponSet->offHand.modelID;});

    GW::GameThread::Enqueue([mainHand = (mainHand != items->end() ? *mainHand : nullptr), offHand = (offHand != items->end() ? *offHand : nullptr)]
    {
        if (mainHand) GW::Items::EquipItem(mainHand);
        if (offHand) GW::Items::EquipItem(offHand);
    });

    return true;
}

void ExtraWeaponsetsWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) 
    {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) 
    {
        return;
    }

    const auto& items = GW::Items::GetItemArray();
    if (!items || weaponSets.empty()) return;
    const auto backGround = GwDatTextureModule::LoadTextureFromFileId(GwDatTextureModule::FileHashToFileId(L"\x3875\x0102"));
    if (!backGround) return;

    const auto columnCount = std::min((size_t)columns, weaponSets.size());
    const auto rowCount = weaponSets.size() / columnCount + 1;
    // const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    ImGui::SetNextWindowSize(ImVec2((float)size * columnCount, (float)size * rowCount));
    if (ImGui::Begin(Name(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) 
    {
        const auto initialPosition = ImGui::GetCursorScreenPos();
        auto column = 0u;
        auto row = 0u;
        for (const auto& weaponSet : weaponSets) 
        {
            const auto position = ImVec2{initialPosition.x + size * column, initialPosition.y + size * row};
            const auto mainHand = std::ranges::find_if(*items, [&](const auto* item) { return weaponSet.mainHand.modelID && item && item->model_id == weaponSet.mainHand.modelID;});
            const auto offHand = std::ranges::find_if(*items, [&](const auto* item) { return weaponSet.offHand.modelID && item && item->model_id == weaponSet.offHand.modelID;});
            
            ImGui::PushID(column + row * columnCount);
            ImGui::GetWindowDrawList()->AddImage(*backGround, position, {position.x + size, position.y + size}, {0, 0}, {.5, 1});
            if (offHand != items->end()) ImGui::GetWindowDrawList()->AddImage(*Resources::GetItemImage(*offHand), {position.x + weaponOffsetFactor * size, position.y}, {position.x + (1 + weaponOffsetFactor) * size, position.y + size});
            if (mainHand != items->end()) ImGui::GetWindowDrawList()->AddImage(*Resources::GetItemImage(*mainHand), {position.x + weaponOffsetFactor * size, position.y}, {position.x + (1 + weaponOffsetFactor) * size, position.y + size});
            ImGui::PopID();
            
            ++column;
            if (column == columnCount) 
            {
                ++row;
                column = 0;
            }
        }
    }
    ImGui::End();
}
