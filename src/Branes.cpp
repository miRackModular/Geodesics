//***********************************************************************************************
//Colliding Sample-and-Hold module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************

//Extra design notes:
/*
Tout fonctionne comme dans le manuel amis j’ai précisé ici les différente type de buits :
IL n’y aurai que 4 generateurs :
1 bruit blanc avec 2 sorties identiques dans la brane inférieure et supérieure
1 bruit bleu avec 4 sorties : 2 identiques dans la brane inférieure et supérieure coté droit et  2 identiques en phase inversée dans la brane inférieure et supérieure coté droit 
1 bruit rouge avec 4 sorties : 2 identiques dans la brane inférieure et supérieure coté droit et  2 identiques en phase inversée dans la brane inférieure et supérieure coté droit
1 bruit rose avec 4 sorties : 2 identiques dans la brane inférieure et supérieure coté droit et  2 identiques en phase inversée dans la brane inférieure et supérieure coté droit

Pour les deux sorties « en collision », il faudra qu’elle reçoive un mélange des deux triggers sources. Je ne sais pas quelle est la meilleure technique pour faire ça. Peut-être de la logique booléenne « and » ?
*/


#include "Geodesics.hpp"


struct Branes : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 14),
		ENUMS(TRIG_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 14),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	
	// Constants
	// none

	// Need to save, with reset
	// none
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	// none
	
	// No need to save, no reset
	// none

	
	Branes() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		
		// No need to save, no reset		
		// none
		
		onReset();
	}

	
	// widgets are not yet created when module is created 
	// even if widgets not created yet, can use params[] and should handle 0.0f value since step may call 
	//   this before widget creation anyways
	// called from the main thread if by constructor, called by engine thread if right-click initialization
	//   when called by constructor, module is created before the first step() is called
	void onReset() override {
		// Need to save, with reset
		// none
		
		// No need to save, with reset
		// none
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		// none 
		
		// No need to save, with reset
		// none
	}

	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// No need to save, with reset
		// none
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		
	}// step()

};


struct BranesWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Branes *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Branes *module = dynamic_cast<Branes*>(this->module);
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

		return menu;
	}	
	
	BranesWidget(Branes *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Branes.svg")));
        //panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Branes_dark.svg")));// no dark pannel for now
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		static constexpr float rowRulerHoldA = 125.5;
		static constexpr float rowRulerHoldB = 254.5f;

	}
};

Model *modelBranes = Model::create<Branes, BranesWidget>("Geodesics", "Branes", "Branes", SAMPLE_AND_HOLD_TAG);

/*CHANGE LOG

0.6.0:
created

*/