#pragma once

#include <GWCA/GameEntities/Item.h>

#include <ToolboxWidget.h>

struct InventoryItem 
{
    uint32_t modelID = 0;
    std::wstring encodedName = L"";
    std::vector<GW::ItemModifier> modifiers = {};
};

struct Hotkey 
{
    long keyData = 0;
    long modifier = 0;
    bool operator==(const Hotkey&) const = default;
};

struct WeaponSet 
{
    // Serialized:
    InventoryItem mainHand;
    InventoryItem offHand;
    Hotkey hotkey;

    // Non-serialized:
    bool isActive = false;
};

class ExtraWeaponsetsWidget : public ToolboxWidget 
{
public:
    static ExtraWeaponsetsWidget& Instance()
    {
        static ExtraWeaponsetsWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "ExtraWeaponsets"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }

    void Initialize() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
    bool OnMouseDown(const UINT, const WPARAM, const LPARAM lParam);

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

private:
    int size = 100;
    int columns = 4;
    std::vector<WeaponSet> weaponSets;
};
