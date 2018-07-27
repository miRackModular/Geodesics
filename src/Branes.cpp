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
	// S&H are numbered 0 to 6 in BraneA from lower left to lower right
	// S&H are numbered 7 to 13 in BraneB from top right to top left
	enum NoiseId {NONE, WHITE, PINK, RED, BLUE};//use negative value for inv phase
	int noiseSources[14] = {PINK, RED, BLUE, WHITE, -BLUE, -RED, -PINK,   -PINK, -RED, -BLUE, WHITE, BLUE, RED, PINK};
	static constexpr float nullNoise = 100.0f;// when a noise has not been generated for the current step

	// Need to save, with reset
	// none
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	float heldOuts[14];
	
	// No need to save, no reset
	SchmittTrigger sampleTriggers[2];

	
	Branes() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		
		// No need to save, no reset		
		sampleTriggers[0].reset();
		sampleTriggers[1].reset();
		
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
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		// none 
		
		// No need to save, with reset
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
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
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		float noises[5] = {nullNoise, nullNoise, nullNoise, nullNoise, nullNoise};// order is none, white, pink, red, blue
		
		bool trigs[2];
		for (int i = 0; i < 2; i++)		
			trigs[i] = sampleTriggers[i].process(inputs[TRIG_INPUTS + i].value);
		
		for (int sh = 0; sh < 14; sh++) {
			// TODO crosstrig mechanism for SH6 and SH13
			if (inputs[TRIG_INPUTS + sh / 7].active) {
				if (trigs[sh / 7]) {
					if (inputs[IN_INPUTS + sh].active)
						heldOuts[sh] = inputs[IN_INPUTS + sh].value;
					else {
						int noiseIndex = abs( noiseSources[sh] );
						if (noises[noiseIndex] == nullNoise) {
							noises[noiseIndex] = randomUniform() * 10.0f - 5.0f; // TODO: support other noises (is white ok using randomUniform()?)
						}
						heldOuts[sh] = noises[noiseIndex] * (noiseSources[sh] > 0 ? 1.0f : -1.0f);
					}
				}
			}
			else { // no trig connected
				if (inputs[IN_INPUTS + sh].active) {
					// ?? (not specified in documentation
				}
				else {
					// Same code as the else active above (get a noise sample)
				}
			}
			outputs[OUT_OUTPUTS + sh].value = heldOuts[sh];
		}
		
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
		static constexpr float radiusIn = 35.0f;
		static constexpr float radiusOut = 64.0f;
		static constexpr float offsetIn = 25.0f;
		static constexpr float offsetOut = 46.0f;
		
		
		// BraneA trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA), Port::INPUT, module, Branes::TRIG_INPUTS + 0, &module->panelTheme));
		
		// BraneA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldA), Port::INPUT, module, Branes::IN_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 2, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusIn), Port::INPUT, module, Branes::IN_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 4, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldA), Port::INPUT, module, Branes::IN_INPUTS + 5, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 6, &module->panelTheme));

		// BraneA outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldA), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 4, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldA), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 5, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 6, &module->panelTheme));
		
		
		// BraneB trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB), Port::INPUT, module, Branes::TRIG_INPUTS + 1, &module->panelTheme));
		
		// BraneB inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 7, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldB), Port::INPUT, module, Branes::IN_INPUTS + 8, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 9, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusIn), Port::INPUT, module, Branes::IN_INPUTS + 10, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 11, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldB), Port::INPUT, module, Branes::IN_INPUTS + 12, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 13, &module->panelTheme));


		// BraneB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 7, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldB), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 8, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 9, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 10, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 11, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldB), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 12, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 13, &module->panelTheme));
	}
};

Model *modelBranes = Model::create<Branes, BranesWidget>("Geodesics", "Branes", "Branes", SAMPLE_AND_HOLD_TAG);

/*CHANGE LOG

0.6.0:
created

*/