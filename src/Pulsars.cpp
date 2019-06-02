//***********************************************************************************************
//Neutron Powered Morphing module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Pulsars : Module {
	enum ParamIds {
		ENUMS(VOID_PARAMS, 2),// push-button
		ENUMS(REV_PARAMS, 2),// push-button
		ENUMS(RND_PARAMS, 2),// push-button
		ENUMS(CVLEVEL_PARAMS, 2),// push-button
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
		ENUMS(CVALEVEL_LIGHTS, 3),// White, but three lights (light 0 is cvMode == 0, light 1 is cvMode == 1, light 2 is cvMode == 2)
		ENUMS(CVBLEVEL_LIGHTS, 3),// Same but for lower pulsar
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float epsilon = 0.0001f;// pulsar crossovers at epsilon and 1-epsilon in 0.0f to 1.0f space

	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int cvModes[2];// 0 is -5v to 5v, 1 is 0v to 10v, 2 is new ALL mode (0-10V); index 0 is upper Pulsar, index 1 is lower Pulsar
	bool isVoid[2];
	bool isReverse[2];
	bool isRandom[2];
	
	// No need to save, with reset
	int connectedA[8];// concatenated list of input indexes of connected ports
	int connectedAnum;
	int connectedB[8];// concatenated list of input indexes of connected ports
	int connectedBnum;
	bool topCross[2];
	int posA;// always between 0 and 7
	int posB;// always between 0 and 7
	int posAnext;// always between 0 and 7
	int posBnext;// always between 0 and 7
	
	
	// No need to save, no reset
	Trigger voidTriggers[2];
	Trigger revTriggers[2];
	Trigger rndTriggers[2];
	Trigger cvLevelTriggers[2];
	float lfoLights[2] = {0.0f, 0.0f};
	RefreshCounter refresh;

	
	void updateConnected() {
		// builds packed list of connected ports for both pulsars
		connectedAnum = 0;
		for (int i = 0; i < 8; i++) {
			if (inputs[INA_INPUTS + i].isConnected()) {
				connectedA[connectedAnum] = i;
				connectedAnum++;
			}
		}
		connectedBnum = 0;
		for (int i = 0; i < 8; i++) {
			if (outputs[OUTB_OUTPUTS + i].isConnected()) {
				connectedB[connectedBnum] = i;
				connectedBnum++;
			}
		}
	}
	void updatePosNext() {
		posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
		posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
	}
	
	
	Pulsars() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(VOID_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar void");
		configParam(REV_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar reverse");	
		configParam(RND_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar random");
		configParam(CVLEVEL_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar uni/bi-polar");	
		configParam(VOID_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar void");
		configParam(REV_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar reverse");	
		configParam(RND_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar random");		
		configParam(CVLEVEL_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar uni/bi-polar");

		onReset();

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		for (int i = 0; i < 2; i++) {
			cvModes[i] = 0;
			isVoid[i] = false;
			isReverse[i] = false;
			isRandom[i] = false;
		}
		resetNonJson();
	}
	void resetNonJson() {
		updateConnected();
		for (int i = 0; i < 2; i++) {
			topCross[i] = false;
		}
		posA = 0;// no need to check isVoid here, will be checked in step()
		posB = 0;// no need to check isVoid here, will be checked in step()
		updatePosNext();
	}

	
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			isVoid[i] = (random::u32() % 2) > 0;
			isReverse[i] = (random::u32() % 2) > 0;
			isRandom[i] = (random::u32() % 2) > 0;
		}
		posA = random::u32() % 8;// no need to check isVoid here, will be checked in step()
		posB = random::u32() % 8;// no need to check isVoid here, will be checked in step()
		updatePosNext();
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// isVoid
		json_object_set_new(rootJ, "isVoid0", json_real(isVoid[0]));
		json_object_set_new(rootJ, "isVoid1", json_real(isVoid[1]));
		
		// isReverse
		json_object_set_new(rootJ, "isReverse0", json_real(isReverse[0]));
		json_object_set_new(rootJ, "isReverse1", json_real(isReverse[1]));
		
		// isRandom
		json_object_set_new(rootJ, "isRandom0", json_real(isRandom[0]));
		json_object_set_new(rootJ, "isRandom1", json_real(isRandom[1]));
		
		// cvMode
		// json_object_set_new(rootJ, "cvMode", json_integer(cvMode));// deprecated
		json_object_set_new(rootJ, "cvMode0", json_integer(cvModes[0]));
		json_object_set_new(rootJ, "cvMode1", json_integer(cvModes[1]));
		
		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// isVoid
		json_t *isVoid0J = json_object_get(rootJ, "isVoid0");
		if (isVoid0J)
			isVoid[0] = json_number_value(isVoid0J);
		json_t *isVoid1J = json_object_get(rootJ, "isVoid1");
		if (isVoid1J)
			isVoid[1] = json_number_value(isVoid1J);

		// isReverse
		json_t *isReverse0J = json_object_get(rootJ, "isReverse0");
		if (isReverse0J)
			isReverse[0] = json_number_value(isReverse0J);
		json_t *isReverse1J = json_object_get(rootJ, "isReverse1");
		if (isReverse1J)
			isReverse[1] = json_number_value(isReverse1J);

		// isRandom
		json_t *isRandom0J = json_object_get(rootJ, "isRandom0");
		if (isRandom0J)
			isRandom[0] = json_number_value(isRandom0J);
		json_t *isRandom1J = json_object_get(rootJ, "isRandom1");
		if (isRandom1J)
			isRandom[1] = json_number_value(isRandom1J);

		// cvMode
		json_t *cvModeJ = json_object_get(rootJ, "cvMode");// legacy
		if (cvModeJ) {
			int cvModeRead = json_integer_value(cvModeJ);
			cvModes[0] = (cvModeRead & 0x1);			
			cvModes[1] = (cvModeRead >> 1);
		}
		else {
			json_t *cvModes0J = json_object_get(rootJ, "cvMode0");
			if (cvModes0J) {
				cvModes[0] = json_integer_value(cvModes0J);
			}
			json_t *cvModes1J = json_object_get(rootJ, "cvMode1");
			if (cvModes1J) {
				cvModes[1] = json_integer_value(cvModes1J);
			}
		}

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {
			// Void, Reverse and Random buttons
			for (int i = 0; i < 2; i++) {
				if (voidTriggers[i].process(params[VOID_PARAMS + i].getValue() + inputs[VOID_INPUTS + i].getVoltage())) {
					isVoid[i] = !isVoid[i];
				}
				if (revTriggers[i].process(params[REV_PARAMS + i].getValue() + inputs[REV_INPUTS + i].getVoltage())) {
					isReverse[i] = !isReverse[i];
				}
				if (rndTriggers[i].process(params[RND_PARAMS + i].getValue())) {// + inputs[RND_INPUTS + i].getVoltage())) {
					isRandom[i] = !isRandom[i];
				}
			}
			
			// CV Level buttons
			for (int i = 0; i < 2; i++) {
				if (cvLevelTriggers[i].process(params[CVLEVEL_PARAMS + i].getValue())) {
					cvModes[i]++;
					if (cvModes[i] > 2)
						cvModes[i] = 0;
				}
			}
			
			updateConnected();
		}// userInputs refresh

		// LFO values (normalized to 0.0f to 1.0f space, inputs clamped and offset adjusted depending cvMode)
		float lfoVal[2];
		lfoVal[0] = inputs[LFO_INPUTS + 0].getVoltage();
		lfoVal[1] = inputs[LFO_INPUTS + 1].isConnected() ? inputs[LFO_INPUTS + 1].getVoltage() : lfoVal[0];
		for (int i = 0; i < 2; i++)
			lfoVal[i] = clamp( (lfoVal[i] + (cvModes[i] == 0 ? 5.0f : 0.0f)) / 10.0f , 0.0f , 1.0f);
		
		
		// Pulsar A
		bool active8[8];
		bool atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = inputs[INA_INPUTS + i].isConnected();
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (cvModes[0] < 2) {
				// regular modes
				if (!isVoid[0]) {
					if (!active8[posA])// ensure start on valid input when no void
						posA = getNextClosestActive(posA, active8, false, false, false);
					if (!active8[posAnext])
						posAnext = getNextClosestActive(posA, active8, false, isReverse[0], isRandom[0]);
				}			
				
				float posPercent = topCross[0] ? (1.0f - lfoVal[0]) : lfoVal[0];
				float nextPosPercent = 1.0f - posPercent;
				outputs[OUTA_OUTPUT].setVoltage(posPercent * inputs[INA_INPUTS + posA].getVoltage() + nextPosPercent * inputs[INA_INPUTS + posAnext].getVoltage());
				for (int i = 0; i < 8; i++)
					lights[MIXA_LIGHTS + i].setBrightness(0.0f + ((i == posA) ? posPercent : 0.0f) + ((i == posAnext) ? nextPosPercent : 0.0f));
				
				// PulsarA crossover (LFO detection)
				if ( (topCross[0] && lfoVal[0] > (1.0f - epsilon)) || (!topCross[0] && lfoVal[0] < epsilon) ) {
					topCross[0] = !topCross[0];// switch to opposite detection
					posA = posAnext;
					posAnext = getNextClosestActive(posA, active8, isVoid[0], isReverse[0], isRandom[0]);
					lfoLights[0] = 1.0f;
				}
			}
			else {
				// new ALL mode
				lfoVal[0] *= (float)connectedAnum;
				int indexA = (int)lfoVal[0];
				float indexANextPercent = lfoVal[0] - (float)indexA;
				int indexANext = (indexA + 1);
				float indexApercent = 1.0f - indexANextPercent;
				if (indexA >= connectedAnum) indexA = 0;
				if (indexANext >= connectedAnum) indexANext = 0;
				outputs[OUTA_OUTPUT].setVoltage(indexApercent * inputs[INA_INPUTS + connectedA[indexA]].getVoltage() + indexANextPercent * inputs[INA_INPUTS + connectedA[indexANext]].getVoltage());
				for (int i = 0; i < 8; i++)
					lights[MIXA_LIGHTS + i].setBrightness(0.0f + ((i == connectedA[indexA]) ? indexApercent : 0.0f) + ((i == connectedA[indexANext]) ? indexANextPercent : 0.0f));
			}
		}
		else {
			outputs[OUTA_OUTPUT].setVoltage(0.0f);
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].setBrightness(0.0f);
		}


		// Pulsar B
		atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = outputs[OUTB_OUTPUTS + i].isConnected();
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (cvModes[0] < 2) {
				// regular modes
				if (!isVoid[1]) {
					if (!active8[posB])// ensure start on valid output when no void
						posB = getNextClosestActive(posB, active8, false, false, false);
					if (!active8[posBnext])
						posBnext = getNextClosestActive(posB, active8, false, isReverse[1], isRandom[1]);
				}			
				
				float posPercent = topCross[1] ? (1.0f - lfoVal[1]) : lfoVal[1];
				float nextPosPercent = 1.0f - posPercent;
				for (int i = 0; i < 8; i++) {
					if (inputs[INB_INPUT].isConnected())
						outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == posB) ? (posPercent * inputs[INB_INPUT].getVoltage()) : 0.0f) + ((i == posBnext) ? (nextPosPercent * inputs[INB_INPUT].getVoltage()) : 0.0f));
					else// mutidimentional trick
						outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == posB) ? (posPercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f) + ((i == posBnext) ? (nextPosPercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f));
					lights[MIXB_LIGHTS + i].setBrightness(0.0f + ((i == posB) ? posPercent : 0.0f) + ((i == posBnext) ? nextPosPercent : 0.0f));
				}
				
				// PulsarB crossover (LFO detection)
				if ( (topCross[1] && lfoVal[1] > (1.0f - epsilon)) || (!topCross[1] && lfoVal[1] < epsilon) ) {
					topCross[1] = !topCross[1];// switch to opposite detection
					posB = posBnext;
					posBnext = getNextClosestActive(posB, active8, isVoid[1], isReverse[1], isRandom[1]);
					lfoLights[1] = 1.0f;
				}
			}
			else {
				// new ALL mode
				lfoVal[1] *= (float)connectedBnum;
				int indexB = (int)lfoVal[1];
				float indexBNextPercent = lfoVal[1] - (float)indexB;
				int indexBNext = (indexB + 1);
				float indexBpercent = 1.0f - indexBNextPercent;
				if (indexB >= connectedBnum) indexB = 0;
				if (indexBNext >= connectedBnum) indexBNext = 0;
				for (int i = 0; i < 8; i++) {
					if (inputs[INB_INPUT].isConnected())
						outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == connectedB[indexB]) ? (indexBpercent * inputs[INB_INPUT].getVoltage()) : 0.0f) + ((i == connectedB[indexBNext]) ? (indexBNextPercent * inputs[INB_INPUT].getVoltage()) : 0.0f));
					else// mutidimentional trick
						outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == connectedB[indexB]) ? (indexBpercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f) + ((i == connectedB[indexBNext]) ? (indexBNextPercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f));
					lights[MIXB_LIGHTS + i].setBrightness(0.0f + ((i == connectedB[indexB]) ? indexBpercent : 0.0f) + ((i == connectedB[indexBNext]) ? indexBNextPercent : 0.0f));
				}
			}
		}
		else {
			for (int i = 0; i < 8; i++) {
				outputs[OUTB_OUTPUTS + i].setVoltage(0.0f);
				lights[MIXB_LIGHTS + i].setBrightness(0.0f);
			}
		}

		
		// lights
		if (refresh.processLights()) {
			// Void, Reverse and Random lights
			for (int i = 0; i < 2; i++) {
				lights[VOID_LIGHTS + i].setBrightness(isVoid[i] ? 1.0f : 0.0f);
				lights[REV_LIGHTS + i].setBrightness(isReverse[i] ? 1.0f : 0.0f);
				lights[RND_LIGHTS + i].setBrightness(isRandom[i] ? 1.0f : 0.0f);
			}
			
			// CV mode (cv level) lights
			for (int i = 0; i < 3; i++) {
				lights[CVALEVEL_LIGHTS + i].setBrightness(cvModes[0] == i ? 1.0f : 0.0f);
				lights[CVBLEVEL_LIGHTS + i].setBrightness(cvModes[1] == i ? 1.0f : 0.0f);
			}
			
			// LFO lights
			for (int i = 0; i < 2; i++) {
				lights[LFO_LIGHTS + i].setSmoothBrightness(lfoLights[i], (float)args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));
				lfoLights[i] = 0.0f;
			}
			
		}// lightRefreshCounter
		
	}// step()
	
	
	int getNextClosestActive(int pos, bool* active8, bool voidd, bool reverse, bool random) {
		// finds the next closest active position (excluding current if active)
		// assumes at least one active, but may not be given pos; will always return an active pos
		// scans all 8 positions
		int posNext = -1;// should never be returned
		if (random) {
			if (voidd)
				posNext = (pos + 1 + random::u32() % 7) % 8;
			else {
				posNext = pos;
				int activeIndexes[8];// room for all indexes of active positions except current if active(max size is guaranteed to be < 8)
				int activeIndexesI = 0;
				for (int i = 0; i < 8; i++) {
					if (active8[i] && i != pos) {
						activeIndexes[activeIndexesI] = i;
						activeIndexesI++;
					}
				}
				if (activeIndexesI > 0)
					posNext = activeIndexes[random::u32()%activeIndexesI];
			}	
		}
		else { 
			posNext = (pos + (reverse ? 7 : 1)) % 8;// void approach by default (choose slot whether active of not)
			if (!voidd) {
				if (reverse) {
					for (int i = posNext + 8; i > posNext; i--) {
						if (active8[i % 8]) {
							posNext = i % 8;
							break;
						}
					}
				}
				else {
					for (int i = posNext; i < posNext + 8; i++) {
						if (active8[i % 8]) {
							posNext = i % 8;
							break;
						}
					}
				}
			}
		}
		return posNext;
	}
};


struct PulsarsWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Pulsars *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Pulsars *module = dynamic_cast<Pulsars*>(this->module);
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

		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
	}	
	
	PulsarsWidget(Pulsars *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Pulsars-WL.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Pulsars-DM.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
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
		static constexpr float offsetLedVsButBX = 8.0f;// BI
		static constexpr float offsetLedVsButBY = 10.0f;
		static constexpr float offsetLedVsButUX = 13.0f;// UNI
		static constexpr float offsetLedVsButUY = 1.0f;


		// PulsarA center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA), false, module, Pulsars::OUTA_OUTPUT, module ? &module->panelTheme : NULL));
		
		// PulsarA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA - radiusJacks), true, module, Pulsars::INA_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA - offsetJacks), true, module, Pulsars::INA_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarA), true, module, Pulsars::INA_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA + offsetJacks), true, module, Pulsars::INA_INPUTS + 3, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA + radiusJacks), true, module, Pulsars::INA_INPUTS + 4, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA + offsetJacks), true, module, Pulsars::INA_INPUTS + 5, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarA), true, module, Pulsars::INA_INPUTS + 6, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA - offsetJacks), true, module, Pulsars::INA_INPUTS + 7, module ? &module->panelTheme : NULL));

		// PulsarA lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarA - radiusLeds), module, Pulsars::MIXA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarA + radiusLeds), module, Pulsars::MIXA_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 7));
		
		// PulsarA void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), true, module, Pulsars::VOID_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::VOID_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::VOID_PARAMS + 0, module ? &module->panelTheme : NULL));

		// PulsarA reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), true, module, Pulsars::REV_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::REV_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::REV_PARAMS + 0, module ? &module->panelTheme : NULL));
		
		// PulsarA random (light and button)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndLedX, rowRulerPulsarA + offsetRndLedY), module, Pulsars::RND_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRndButtonX, rowRulerPulsarA + offsetRndButtonY), module, Pulsars::RND_PARAMS + 0, module ? &module->panelTheme : NULL));

		// PulsarA CV level (lights and button)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRndButtonX, rowRulerPulsarA + offsetRndButtonY), module, Pulsars::CVLEVEL_PARAMS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndButtonX - offsetLedVsButBX, rowRulerPulsarA + offsetRndButtonY + offsetLedVsButBY), module, Pulsars::CVALEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndButtonX - offsetLedVsButUX, rowRulerPulsarA + offsetRndButtonY - offsetLedVsButUY), module, Pulsars::CVALEVEL_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndButtonX - offsetLedVsButUX -10, rowRulerPulsarA + offsetRndButtonY - offsetLedVsButUY + 10), module, Pulsars::CVALEVEL_LIGHTS + 2));

		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA + offsetJacks + offsetLFO), true, module, Pulsars::LFO_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 0));
		
		
		// PulsarB center input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB), true, module, Pulsars::INB_INPUT, module ? &module->panelTheme : NULL));
		
		// PulsarB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB - radiusJacks), false, module, Pulsars::OUTB_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB - offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarB), false, module, Pulsars::OUTB_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB + offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB + radiusJacks), false, module, Pulsars::OUTB_OUTPUTS + 4, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB + offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 5, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarB), false, module, Pulsars::OUTB_OUTPUTS + 6, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB - offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 7, module ? &module->panelTheme : NULL));

		// PulsarB lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarB - radiusLeds), module, Pulsars::MIXB_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarB + radiusLeds), module, Pulsars::MIXB_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 7));
		
		// PulsarB void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), true, module, Pulsars::VOID_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::VOID_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::VOID_PARAMS + 1, module ? &module->panelTheme : NULL));

		// PulsarB reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), true, module, Pulsars::REV_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::REV_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::REV_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// PulsarB random (light and button)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndLedX, rowRulerPulsarB - offsetRndLedY), module, Pulsars::RND_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRndButtonX, rowRulerPulsarB - offsetRndButtonY), module, Pulsars::RND_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// PulsarB CV level (lights and button)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRndButtonX, rowRulerPulsarB - offsetRndButtonY), module, Pulsars::CVLEVEL_PARAMS + 1,  module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndButtonX + offsetLedVsButBX, rowRulerPulsarB - offsetRndButtonY - offsetLedVsButBY), module, Pulsars::CVBLEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndButtonX + offsetLedVsButUX, rowRulerPulsarB - offsetRndButtonY + offsetLedVsButUY), module, Pulsars::CVBLEVEL_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndButtonX + offsetLedVsButUX + 10, rowRulerPulsarB - offsetRndButtonY + offsetLedVsButUY - 10), module, Pulsars::CVBLEVEL_LIGHTS + 2));

		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB - offsetJacks - offsetLFO), true, module, Pulsars::LFO_INPUTS + 1, module ? &module->panelTheme : NULL));		
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 1));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Pulsars*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Pulsars*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelPulsars = createModel<Pulsars, PulsarsWidget>("Pulsars");

/*CHANGE LOG


*/