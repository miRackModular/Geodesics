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
		ENUMS(VOID_PARAMS, 2),// push-button
		ENUMS(REV_PARAMS, 2),// push-button
		ENUMS(RND_PARAMS, 2),// push-button
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INA_INPUTS, 8),
		INB_INPUT,
		ENUMS(LFO_INPUTS, 2),
		ENUMS(VOID_INPUTS, 2),
		ENUMS(REV_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		OUTA_OUTPUT,
		ENUMS(OUTB_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(LFO_LIGHTS, 2),
		ENUMS(MIXA_LIGHTS, 8),
		ENUMS(MIXB_LIGHTS, 8),
		ENUMS(VOID_LIGHTS, 2),
		ENUMS(REV_LIGHTS, 2),
		ENUMS(RND_LIGHTS, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float epsilon = 0.0001f;// pulsar crossovers at epsilon and 1-epsilon in 0.0f to 1.0f space

	// Need to save, with reset
	bool isVoid[2];
	bool isReverse[2];
	bool isRandom[2];
	
	// Need to save, no reset
	int panelTheme;
	int cvMode;
	
	// No need to save, with reset
	int posA;// always between 0 and 7
	int posB;// always between 0 and 7
	int posAnext;// always between 0 and 7
	int posBnext;// always between 0 and 7
	bool topCross[2];
	
	// No need to save, no reset
	SchmittTrigger voidTriggers[2];
	SchmittTrigger revTriggers[2];
	SchmittTrigger rndTriggers[2];
	float lfoLights[2];

	
	Branes() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		cvMode = 0;// 0 is -5v to 5v, 1 is 0v to 10v
		
		// No need to save, no reset		
		for (int i = 0; i < 2; i++) {
			lfoLights[i] = 0.0f;
			voidTriggers[i].reset();
			revTriggers[i].reset();
			rndTriggers[i].reset();
		}
		
		onReset();
	}

	
	// widgets are not yet created when module is created 
	// even if widgets not created yet, can use params[] and should handle 0.0f value since step may call 
	//   this before widget creation anyways
	// called from the main thread if by constructor, called by engine thread if right-click initialization
	//   when called by constructor, module is created before the first step() is called
	void onReset() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++) {
			topCross[i] = false;
			isVoid[i] = false;
			isReverse[i] = false;
			isRandom[i] = false;
		}
		// No need to save, with reset
		posA = 0;// no need to check isVoid here, will be checked in step()
		posB = 0;// no need to check isVoid here, will be checked in step()
		posAnext = 1;// no need to check isVoid here, will be checked in step()
		posBnext = 1;// no need to check isVoid here, will be checked in step()
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++) {
			isVoid[i] = (randomu32() % 2) > 0;
			isReverse[i] = (randomu32() % 2) > 0;
			isRandom[i] = (randomu32() % 2) > 0;
		}
		// No need to save, with reset
		posA = randomu32() % 8;// no need to check isVoid here, will be checked in step()
		posB = randomu32() % 8;// no need to check isVoid here, will be checked in step()
		posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
		posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
	}

	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// isVoid
		json_object_set_new(rootJ, "isVoid0", json_real(isVoid[0]));
		json_object_set_new(rootJ, "isVoid1", json_real(isVoid[1]));
		
		// isReverse
		json_object_set_new(rootJ, "isReverse0", json_real(isReverse[0]));
		json_object_set_new(rootJ, "isReverse1", json_real(isReverse[1]));
		
		// isRandom
		json_object_set_new(rootJ, "isRandom0", json_real(isRandom[0]));
		json_object_set_new(rootJ, "isRandom1", json_real(isRandom[1]));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cvMode
		json_object_set_new(rootJ, "cvMode", json_integer(cvMode));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// isVoid
		json_t *isVoid0J = json_object_get(rootJ, "isVoid0");
		if (isVoid0J)
			isVoid[0] = json_real_value(isVoid0J);
		json_t *isVoid1J = json_object_get(rootJ, "isVoid1");
		if (isVoid1J)
			isVoid[1] = json_real_value(isVoid1J);

		// isReverse
		json_t *isReverse0J = json_object_get(rootJ, "isReverse0");
		if (isReverse0J)
			isReverse[0] = json_real_value(isReverse0J);
		json_t *isReverse1J = json_object_get(rootJ, "isReverse1");
		if (isReverse1J)
			isReverse[1] = json_real_value(isReverse1J);

		// isRandom
		json_t *isRandom0J = json_object_get(rootJ, "isRandom0");
		if (isRandom0J)
			isRandom[0] = json_real_value(isRandom0J);
		json_t *isRandom1J = json_object_get(rootJ, "isRandom1");
		if (isRandom1J)
			isRandom[1] = json_real_value(isRandom1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// cvMode
		json_t *cvModeJ = json_object_get(rootJ, "cvMode");
		if (cvModeJ)
			cvMode = json_integer_value(cvModeJ);

		// No need to save, with reset
		posA = 0;// no need to check isVoid here, will be checked in step()
		posB = 0;// no need to check isVoid here, will be checked in step()
		posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
		posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
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
		static constexpr float rowRulerPulsarA = 127.5;
		static constexpr float rowRulerPulsarB = 261.5f;
		float rowRulerLFOlights = (rowRulerPulsarA + rowRulerPulsarB) / 2.0f;
		static constexpr float offsetLFOlightsX = 25.0f;
		static constexpr float radiusLeds = 23.0f;
		static constexpr float radiusJacks = 46.0f;
		static constexpr float offsetLeds = 17.0f;
		static constexpr float offsetJacks = 33.0f;
		static constexpr float offsetLFO = 24.0f;// used also for void and reverse CV input jacks
		static constexpr float offsetLedX = 13.0f;// adds/subs to offsetLFO for void and reverse lights
		static constexpr float offsetButtonX = 26.0f;// adds/subs to offsetLFO for void and reverse buttons
		static constexpr float offsetLedY = 11.0f;// adds/subs to offsetLFO for void and reverse lights
		static constexpr float offsetButtonY = 18.0f;// adds/subs to offsetLFO for void and reverse buttons
		static constexpr float offsetRndButtonX = 58.0f;// from center of pulsar
		static constexpr float offsetRndButtonY = 24.0f;// from center of pulsar
		static constexpr float offsetRndLedX = 63.0f;// from center of pulsar
		static constexpr float offsetRndLedY = 11.0f;// from center of pulsar

		// PulsarA center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA), Port::OUTPUT, module, Branes::OUTA_OUTPUT, &module->panelTheme));
		
		// PulsarA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA - radiusJacks), Port::INPUT, module, Branes::INA_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA - offsetJacks), Port::INPUT, module, Branes::INA_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarA), Port::INPUT, module, Branes::INA_INPUTS + 2, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA + offsetJacks), Port::INPUT, module, Branes::INA_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA + radiusJacks), Port::INPUT, module, Branes::INA_INPUTS + 4, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA + offsetJacks), Port::INPUT, module, Branes::INA_INPUTS + 5, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarA), Port::INPUT, module, Branes::INA_INPUTS + 6, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA - offsetJacks), Port::INPUT, module, Branes::INA_INPUTS + 7, &module->panelTheme));

		// PulsarA lights
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarA - radiusLeds), module, Branes::MIXA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA - offsetLeds), module, Branes::MIXA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarA), module, Branes::MIXA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA + offsetLeds), module, Branes::MIXA_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarA + radiusLeds), module, Branes::MIXA_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA + offsetLeds), module, Branes::MIXA_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarA), module, Branes::MIXA_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA - offsetLeds), module, Branes::MIXA_LIGHTS + 7));
		
		// PulsarA void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), Port::INPUT, module, Branes::VOID_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Branes::VOID_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Branes::VOID_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));

		// PulsarA reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), Port::INPUT, module, Branes::REV_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Branes::REV_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Branes::REV_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// PulsarA random (light and button)
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetRndLedX, rowRulerPulsarA + offsetRndLedY), module, Branes::RND_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRndButtonX, rowRulerPulsarA + offsetRndButtonY), module, Branes::RND_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));

		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA + offsetJacks + offsetLFO), Port::INPUT, module, Branes::LFO_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetLFOlightsX, rowRulerLFOlights), module, Branes::LFO_LIGHTS + 0));
		
		
		// PulsarB center input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB), Port::INPUT, module, Branes::INB_INPUT, &module->panelTheme));
		
		// PulsarB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB - radiusJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB - offsetJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarB), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB + offsetJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB + radiusJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 4, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB + offsetJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 5, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarB), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 6, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB - offsetJacks), Port::OUTPUT, module, Branes::OUTB_OUTPUTS + 7, &module->panelTheme));

		// PulsarB lights
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB - radiusLeds), module, Branes::MIXB_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB - offsetLeds), module, Branes::MIXB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarB), module, Branes::MIXB_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB + offsetLeds), module, Branes::MIXB_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB + radiusLeds), module, Branes::MIXB_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB + offsetLeds), module, Branes::MIXB_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarB), module, Branes::MIXB_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB - offsetLeds), module, Branes::MIXB_LIGHTS + 7));
		
		// PulsarB void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), Port::INPUT, module, Branes::VOID_INPUTS + 1, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Branes::VOID_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Branes::VOID_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));

		// PulsarB reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), Port::INPUT, module, Branes::REV_INPUTS + 1, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Branes::REV_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Branes::REV_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// PulsarB random (light and button)
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetRndLedX, rowRulerPulsarB - offsetRndLedY), module, Branes::RND_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRndButtonX, rowRulerPulsarB - offsetRndButtonY), module, Branes::RND_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB - offsetJacks - offsetLFO), Port::INPUT, module, Branes::LFO_INPUTS + 1, &module->panelTheme));		
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetLFOlightsX, rowRulerLFOlights), module, Branes::LFO_LIGHTS + 1));
	}
};

Model *modelBranes = Model::create<Branes, BranesWidget>("Geodesics", "Branes", "Branes", SAMPLE_AND_HOLD_TAG);

/*CHANGE LOG

0.6.0:
created

*/