//***********************************************************************************************
//Blank-Panel Info for VCV Rack by Pierre Collard and Marc Boulé
//***********************************************************************************************


#include "Geodesics.hpp"


struct BlankInfo : Module {

	int panelTheme;


	BlankInfo() {
		config(0, 0, 0, 0);
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
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

	
	void process(const ProcessArgs &args) override {
	}
};


struct BlankInfoWidget : ModuleWidget {
	BlankInfoWidget(BlankInfo *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/BlankInfo-DM.svg")));
		
		// Screws
		// part of svg panel, no code required
	}
};

Model *modelBlankInfo = createModel<BlankInfo, BlankInfoWidget>("Blank-PanelInfo");

/*CHANGE LOG

1.0.0:
slug changed from "Blank-Panel Info" to "Blank-PanelInfo"

*/
