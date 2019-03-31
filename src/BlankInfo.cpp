//***********************************************************************************************
//Blank-Panel Info for VCV Rack by Pierre Collard and Marc Boulé
//***********************************************************************************************


#include "Geodesics.hpp"


struct BlankInfo : Module {

	int panelTheme = 0;


	BlankInfo() {
		config(0, 0, 0, 0);
		
		onReset();
	}

	void onReset() override {
	}

	void onRandomize() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void process(const ProcessArgs &args) override {
	}
};


struct BlankInfoWidget : ModuleWidget {
	SvgPanel* lightPanel;
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		BlankInfo *module;
		int theme;
		void onAction(const widget::ActionEvent &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		BlankInfo *module = dynamic_cast<BlankInfo*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// Geodesics.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// Geodesics.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);
	}	


	BlankInfoWidget(BlankInfo *module) {
		setModule(module);

		// Main panels from Inkscape
        lightPanel = new SvgPanel();
        lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/BlankInfo-WL.svg")));
        box.size = lightPanel->box.size;
        addChild(lightPanel);
        darkPanel = new SvgPanel();
		darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/BlankInfo-DM.svg")));
		darkPanel->visible = false;
		addChild(darkPanel);

		// Screws
		// part of svg panel, no code required
	}
	
	void step() override {
		if (module) {
			lightPanel->visible = ((((BlankInfo*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((BlankInfo*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelBlankInfo = createModel<BlankInfo, BlankInfoWidget>("Blank-Panel Info");
