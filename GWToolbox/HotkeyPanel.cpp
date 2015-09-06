#include <algorithm>

#include "HotkeyPanel.h"
#include "logger.h"
#include "GWToolbox.h"
#include "Config.h"

HotkeyPanel::HotkeyPanel() {
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();
	hotkeys = vector<TBHotkey*>();
}

void HotkeyPanel::OnMouseScroll(const MouseMessage &mouse) {
	int delta = mouse.GetDelta() > 0 ? 1 : -1;
	scrollbar_->SetValue(scrollbar_->GetValue() + delta);
}

void HotkeyPanel::buildUI() {
	LOG("Building Hotkey Panel\n");
	const int height = 300;
	loadIni();
	
	ScrollBar* scrollbar = new ScrollBar();
	scrollbar->SetLocation(TBHotkey::WIDTH + 2 * DefaultBorderPadding, 0);
	scrollbar->SetSize(scrollbar->GetWidth(), height);
	scrollbar->GetScrollEvent() += ScrollEventHandler([this, scrollbar](Control*, ScrollEventArgs) {
		this->set_first_shown(scrollbar->GetValue());
	});
	scrollbar_ = scrollbar;
	AddControl(scrollbar);
	
	ComboBox* create_combo = new ComboBox();
	create_combo->AddItem("Create Hotkey...");
	create_combo->AddItem("Send Chat");
	create_combo->AddItem("Use Item");
	create_combo->AddItem("Drop or Use Buff");
	create_combo->AddItem("Function Toggle");
	create_combo->AddItem("Target");
	create_combo->AddItem("Move to coordinate");
	create_combo->AddItem("Dialog");
	create_combo->SetSelectedIndex(0);
	create_combo->SetMaxShowItems(create_combo->GetItemsCount());
	create_combo->SetLocation(DefaultBorderPadding,
		DefaultBorderPadding + MAX_SHOWN * (TBHotkey::HEIGHT + DefaultBorderPadding));
	create_combo->SetSize(TBHotkey::WIDTH / 2 - TBHotkey::HSPACE / 2, TBHotkey::LINE_HEIGHT);
	create_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, create_combo](Control*) {
		if (create_combo->GetSelectedIndex() == 0) return;

		wstring ini = L"hotkey-";
		ini += to_wstring(this->NewID());
		ini += L":";
		switch (create_combo->GetSelectedIndex()) {
		case 1:
			ini += HotkeySendChat::IniSection();
			this->AddHotkey(new HotkeySendChat(Key::None, Key::None, true, ini, L"", L'/'));
			break;
		case 2:
			ini += HotkeyUseItem::IniSection();
			this->AddHotkey(new HotkeyUseItem(Key::None, Key::None, true, ini, 0, L""));
			break;
		case 3:
			ini += HotkeyDropUseBuff::IniSection();
			this->AddHotkey(new HotkeyDropUseBuff(Key::None, Key::None, true, ini, GwConstants::SkillID::Recall));
			break;
		case 4:
			ini += HotkeyToggle::IniSection();
			this->AddHotkey(new HotkeyToggle(Key::None, Key::None, true, ini, 1));
			break;
		case 5:
			ini += HotkeyTarget::IniSection();
			this->AddHotkey(new HotkeyTarget(Key::None, Key::None, true, ini, 0, L""));
			break;
		case 6:
			ini += HotkeyMove::IniSection();
			this->AddHotkey(new HotkeyMove(Key::None, Key::None, true, ini, 0.0, 0.0, L""));
			break;
		case 7:
			ini += HotkeyDialog::IniSection();
			this->AddHotkey(new HotkeyDialog(Key::None, Key::None, true, ini, 0, L""));
			break;
		default:
			break;
		}
		create_combo->SetSelectedIndex(0);
	});
	AddControl(create_combo);

	ComboBox* delete_combo = new ComboBox();
	delete_combo->SetSize(TBHotkey::WIDTH / 2 - TBHotkey::HSPACE / 2, TBHotkey::LINE_HEIGHT);
	delete_combo->SetLocation(create_combo->GetRight() + TBHotkey::HSPACE, create_combo->GetTop());
	delete_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, delete_combo](Control*) {
		int index = delete_combo->GetSelectedIndex();
		if (index == 0) return;
		this->DeleteHotkey(index - 1);
	});
	AddControl(delete_combo);
	delete_combo_ = delete_combo;
	
	for (TBHotkey* hotkey : hotkeys) {
		AddSubControl(hotkey);
	}
	
	first_shown_ = 0;
	UpdateScrollBarMax();
	scrollbar_->ScrollToTop();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();

	SetSize(TBHotkey::WIDTH + 2 * DefaultBorderPadding + scrollbar->GetWidth(), height);
}

void HotkeyPanel::UpdateDeleteCombo() {
	delete_combo_->Clear();
	delete_combo_->AddItem("Delete Hotkey...");
	for (int i = 0; i < (int)hotkeys.size(); ++i) {
		delete_combo_->AddItem(hotkeys[i]->GetDescription());
	}
	delete_combo_->SetSelectedIndex(0);
}

void HotkeyPanel::UpdateScrollBarMax() {
	int max = (std::max)(0, (int)hotkeys.size() - MAX_SHOWN);
	scrollbar_->SetMaximum(max);
}

void HotkeyPanel::DeleteHotkey(int index) {
	if (index < 0 || index >= (int)hotkeys.size()) return;

	GWToolbox::instance()->config()->iniDeleteSection(hotkeys[index]->ini_section().c_str());
	RemoveControl(hotkeys[index]);
	hotkeys.erase(hotkeys.begin() + index);
	UpdateScrollBarMax();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();
}

void HotkeyPanel::AddHotkey(TBHotkey* hotkey) {
	hotkeys.push_back(hotkey);
	AddSubControl(hotkey);
	UpdateScrollBarMax();
	scrollbar_->ScrollToBottom();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();
}

void HotkeyPanel::set_first_shown(int first) {
	if (first < 0) {
		first_shown_ = 0;
	} else if (first > (int)hotkeys.size() - MAX_SHOWN) {
		first_shown_ = (int)hotkeys.size() - MAX_SHOWN;
	} else {
		first_shown_ = first;
	}
	
	CalculateHotkeyPositions();
}

void HotkeyPanel::CalculateHotkeyPositions() {
	if (first_shown_ < 0) {
		LOG("ERROR first shown < 0 (HotkeyPanel::CalculateHotkeyPositions())\n");
		first_shown_ = 0;
	}

	for (int i = 0; i < MAX_SHOWN && first_shown_ + i < (int)hotkeys.size(); ++i) {
		hotkeys[first_shown_ + i]->SetLocation(DefaultBorderPadding,
			DefaultBorderPadding + i * (TBHotkey::HEIGHT + DefaultBorderPadding));
	}
}

void HotkeyPanel::DrawSelf(Drawing::RenderContext& context) {
	Panel::DrawSelf(context);

	int i = first_shown_ + MAX_SHOWN - 1;
	if (i >= (int)hotkeys.size()) i = (int)hotkeys.size() - 1;
	for (; i >= first_shown_; --i) {
		hotkeys[i]->Render();
	}
}

bool HotkeyPanel::ProcessMessage(LPMSG msg) {
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		Key modifier = Key::None;
		if (GetKeyState(static_cast<int>(Key::ControlKey)) < 0)
			modifier |= Key::Control;
		if (GetKeyState(static_cast<int>(Key::ShiftKey)) < 0)
			modifier |= Key::Shift;
		if (GetKeyState(static_cast<int>(Key::Menu)) < 0)
			modifier |= Key::Alt;

		Key keyData = (Key)msg->wParam;

		bool triggered = false;
		for (TBHotkey* hk : hotkeys) {
			if (hk->active() 
				&& !hk->pressed() && keyData == hk->key() 
				&& modifier == hk->modifier()) {

				hk->set_pressed(true);
				hk->exec();
				triggered = true;
			}
		}
		return triggered;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP: {
		Key keyData = (Key)msg->wParam;
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && keyData == hk->key()) {
				hk->set_pressed(false);
			}
		}
		return false;
	}

	default:
		return false;
	}
}


void HotkeyPanel::loadIni() {
	Config* config = GWToolbox::instance()->config();

	list<wstring> sections = config->iniReadSections();
	for (wstring section : sections) {
		if (section.compare(0, 6, L"hotkey") == 0) {
			size_t first_sep = 6;
			size_t second_sep = section.find(L':', first_sep);

			wstring id = section.substr(first_sep + 1, second_sep - first_sep - 1);
			try {
				long long_id = stol(id);
				if (long_id > max_id_) max_id_ = long_id;
			} catch (...) {}
			wstring type = section.substr(second_sep + 1);
			//wstring wname = config->iniRead(section.c_str(), L"name", L"");
			//string name = string(wname.begin(), wname.end()); // transform wstring in string
			bool active = config->iniReadBool(section.c_str(), TBHotkey::IniKeyActive(), false);
			Key key = (Key)config->iniReadLong(section.c_str(), TBHotkey::IniKeyHotkey(), 0);
			Key modifier = (Key)config->iniReadLong(section.c_str(), TBHotkey::IniKeyModifier(), 0);
			TBHotkey* tb_hk = NULL;

			if (type.compare(HotkeySendChat::IniSection()) == 0) {
				wstring msg = config->iniRead(section.c_str(), HotkeySendChat::IniKeyMsg(), L"");
				wchar_t channel = config->iniRead(section.c_str(), HotkeySendChat::IniKeyChannel(), L"")[0];
				tb_hk = new HotkeySendChat(key, modifier, active, section, msg, channel);

			} else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
				UINT itemID = (UINT)config->iniReadLong(section.c_str(), HotkeyUseItem::IniItemIDKey(), 0);
				wstring item_name = config->iniRead(section.c_str(), HotkeyUseItem::IniItemNameKey(), L"");
				tb_hk = new HotkeyUseItem(key, modifier, active, section, itemID, item_name);

			} else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
				UINT skillID = (UINT)config->iniReadLong(section.c_str(), HotkeyDropUseBuff::IniSkillIDKey(), 0);
				tb_hk = new HotkeyDropUseBuff(key, modifier, active, section, skillID);

			} else if (type.compare(HotkeyToggle::IniSection()) == 0) {
				int toggleID = (int)config->iniReadLong(section.c_str(), HotkeyToggle::IniToggleIDKey(), 0);
				tb_hk = new HotkeyToggle(key, modifier, active, section, toggleID);

			} else if (type.compare(HotkeyTarget::IniSection()) == 0) {
				UINT targetID = (UINT)config->iniReadLong(section.c_str(), HotkeyTarget::IniTargetIDKey(), 0);
				wstring target_name = config->iniRead(section.c_str(), HotkeyTarget::IniTargetNameKey(), L"");
				tb_hk = new HotkeyTarget(key, modifier, active, section, targetID, target_name);

			} else if (type.compare(HotkeyMove::IniSection()) == 0) {
				float x = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniXKey(), 0.0);
				float y = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniYKey(), 0.0);
				wstring name = config->iniRead(section.c_str(), HotkeyMove::IniNameKey(), L"");
				tb_hk = new HotkeyMove(key, modifier, active, section, x, y, name);

			} else if (type.compare(HotkeyDialog::IniSection()) == 0) {
				UINT dialogID = (UINT)config->iniReadLong(section.c_str(), HotkeyDialog::IniDialogIDKey(), 0);
				wstring dialog_name = config->iniRead(section.c_str(), HotkeyDialog::IniDialogNameKey(), L"");
				tb_hk = new HotkeyDialog(key, modifier, active, section, dialogID, dialog_name);

			} else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
				UINT index = (UINT)config->iniReadLong(section.c_str(), HotkeyPingBuild::IniBuildIdxKey(), 0);
				tb_hk = new HotkeyPingBuild(key, modifier, active, section, index);

			} else {
				LOG("WARNING hotkey detected, but could not match any type!\n");
			}

			if (tb_hk) {
				hotkeys.push_back(tb_hk);
			}
		}
	}
}

void HotkeyPanel::mainRoutine() {
	// TODO clicker

	// TODO coin dropper

	// TODO rupt?
}