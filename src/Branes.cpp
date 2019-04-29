//***********************************************************************************************
//Colliding Sample and Hold module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//Also based on code from Joel Robichaud's Nohmad Noise module
//See ./LICENSE.txt for all licenses
//
//***********************************************************************************************


#include <dsp/filter.hpp>
#include <random>
#include "Geodesics.hpp"


// By Joel Robichaud - Nohmad Noise module
struct NoiseGenerator {
	std::mt19937 rng;
	std::uniform_real_distribution<float> uniform;

	NoiseGenerator() : uniform(-1.0f, 1.0f) {
		rng.seed(std::random_device()());
	}

	float white() {
		return uniform(rng);
	}
};


//*****************************************************************************


// By Joel Robichaud - Nohmad Noise module
struct PinkFilter {
	float b0, b1, b2, b3, b4, b5, b6; // Coefficients
	float y; // Out

	void process(float x) {
		b0 = 0.99886f * b0 + x * 0.0555179f;
		b1 = 0.99332f * b1 + x * 0.0750759f;
		b2 = 0.96900f * b2 + x * 0.1538520f;
		b3 = 0.86650f * b3 + x * 0.3104856f;
		b4 = 0.55000f * b4 + x * 0.5329522f;
		b5 = -0.7616f * b5 - x * 0.0168980f;
		y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362f;
		b6 = x * 0.115926f;
	}

	float pink() {
		return y;
	}
};
// The coefficients above seem to be from this source:
// http://www.firstpr.com.au/dsp/pink-noise/#Filtering


//*****************************************************************************


struct Branes : Module {
	enum ParamIds {
		ENUMS(TRIG_BYPASS_PARAMS, 2),
		// -- 0.6.3 ^^
		ENUMS(NOISE_RANGE_PARAMS, 2),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 14),
		ENUMS(TRIG_INPUTS, 2),
		ENUMS(TRIG_BYPASS_INPUTS, 2),
		// -- 0.6.3 ^^
		ENUMS(NOISE_RANGE_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 14),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(UNUSED1, 2 * 2),// no longer used
		ENUMS(BYPASS_TRIG_LIGHTS, 2 * 2),// room for white-red
		// -- 0.6.3 ^^
		ENUMS(NOISE_RANGE_LIGHTS, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	// S&H are numbered 0 to 6 in BraneA from lower left to lower right
	// S&H are numbered 7 to 13 in BraneB from top right to top left
	enum NoiseId {NONE, WHITE, PINK, RED, BLUE};//use negative value for inv phase
	int noiseSources[14] = {PINK, RED, BLUE, WHITE, BLUE, RED, PINK,   PINK, RED, BLUE, WHITE, BLUE, RED, PINK};

	
	// Need to save, with reset
	int panelTheme = 0;
	bool trigBypass[2];
	bool noiseRange[2];
	
	
	// No need to save
	float heldOuts[14];
	
	
	// No need to save
	Trigger sampleTriggers[2];
	Trigger trigBypassTriggers[2];
	Trigger noiseRangeTriggers[2];
	float trigLights[2] = {0.0f, 0.0f};
	NoiseGenerator whiteNoise;
	PinkFilter pinkFilter[2];
	dsp::RCFilter redFilter[2];
	dsp::RCFilter blueFilter[2];
	PinkFilter pinkForBlueFilter[2];
	bool cacheHitRed[2];// no need to init; index is braneIndex
	float cacheValRed[2];
	bool cacheHitBlue[2];// no need to init; index is braneIndex
	float cacheValBlue[2];
	bool cacheHitPink[2];// no need to init; index is braneIndex
	float cacheValPink[2];
	unsigned int lightRefreshCounter = 0;

	
	Branes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(TRIG_BYPASS_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top brane bypass");
		configParam(TRIG_BYPASS_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom brane bypass");
		configParam(NOISE_RANGE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top brane noise range");
		configParam(NOISE_RANGE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom brane noise range");		
		
		float sampleRate = APP->engine->getSampleRate();
		redFilter[0].setCutoff(441.0f / sampleRate);
		redFilter[1].setCutoff(441.0f / sampleRate);
		blueFilter[0].setCutoff(44100.0f / sampleRate);
		blueFilter[1].setCutoff(44100.0f / sampleRate);
		
		onReset();
	}

	
	void onReset() override {
		for (int i = 0; i < 2; i++) {
			trigBypass[i] = false;
			noiseRange[i] = false;
		}
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			trigBypass[i] = (random::u32() % 2) > 0;
			noiseRange[i] = (random::u32() % 2) > 0;
		}
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// trigBypass
		json_object_set_new(rootJ, "trigBypass0", json_real(trigBypass[0]));
		json_object_set_new(rootJ, "trigBypass1", json_real(trigBypass[1]));

		// noiseRange
		json_object_set_new(rootJ, "noiseRange0", json_real(noiseRange[0]));
		json_object_set_new(rootJ, "noiseRange1", json_real(noiseRange[1]));

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// trigBypass
		json_t *trigBypass0J = json_object_get(rootJ, "trigBypass0");
		if (trigBypass0J)
			trigBypass[0] = json_number_value(trigBypass0J);
		json_t *trigBypass1J = json_object_get(rootJ, "trigBypass1");
		if (trigBypass1J)
			trigBypass[1] = json_number_value(trigBypass1J);

		// noiseRange
		json_t *noiseRange0J = json_object_get(rootJ, "noiseRange0");
		if (noiseRange0J)
			noiseRange[0] = json_number_value(noiseRange0J);
		json_t *noiseRange1J = json_object_get(rootJ, "noiseRange1");
		if (noiseRange1J)
			noiseRange[1] = json_number_value(noiseRange1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	void process(const ProcessArgs &args) override {
	
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
			
			// trigBypass buttons and cv inputs
			for (int i = 0; i < 2; i++) {
				if (trigBypassTriggers[i].process(params[TRIG_BYPASS_PARAMS + i].value + inputs[TRIG_BYPASS_INPUTS + i].getVoltage())) {
					trigBypass[i] = !trigBypass[i];
				}
			}
			
			// noiseRange buttons and cv inputs
			for (int i = 0; i < 2; i++) {
				if (noiseRangeTriggers[i].process(params[NOISE_RANGE_PARAMS + i].value + inputs[NOISE_RANGE_INPUTS + i].getVoltage())) {
					noiseRange[i] = !noiseRange[i];
				}
			}
		
		}// userInputs refresh

		// trig inputs
		bool trigs[2];
		bool trigInputsActive[2];
		for (int i = 0; i < 2; i++)	{	
			trigs[i] = sampleTriggers[i].process(inputs[TRIG_INPUTS + i].getVoltage());
			if (trigs[i])
				trigLights[i] = 1.0f;
			trigInputsActive[i] = trigBypass[i] ? false : inputs[TRIG_INPUTS + i].isConnected();
		}
		
		for (int i = 0; i < 2; i++) {
			cacheHitRed[i] = false;
			cacheHitBlue[i] = false;
			cacheHitPink[i] = false;
		}
		
		// detect unused brane and avoid noise when so (unused = not a single output connected)
		int startSh = 7;
		int endSh = 7;
		for (int sh = 0; sh < 7; sh++) {
			if (outputs[OUT_OUTPUTS + sh].isConnected()) {
				startSh = 0;
				break;
			}
		}
		for (int sh = 7; sh < 14; sh++) {
			if (outputs[OUT_OUTPUTS + sh].isConnected()) {
				endSh = 14;
				break;
			}
		}
			
		
		// sample and hold outputs (noise continually generated or else stepping non-white on S&H only will not work well because of filters)
		float noises[14];
		for (int sh = startSh; sh < endSh; sh++) {
			noises[sh] = getNoise(sh);
			if (outputs[OUT_OUTPUTS + sh].isConnected()) {
				int braneIndex = sh < 7 ? 0 : 1;
				if (trigInputsActive[braneIndex] || (sh == 13 && trigInputsActive[0]) || (sh == 6 && trigInputsActive[1])) {// if trigs connected (with crosstrigger mechanism)
					if ((trigInputsActive[braneIndex] && trigs[braneIndex]) || (sh == 13 && trigInputsActive[0] && trigs[0]) || (sh == 6 && trigInputsActive[1] && trigs[1])) {// if trig rising edge
						if (inputs[IN_INPUTS + sh].isConnected())// if input cable
							heldOuts[sh] = inputs[IN_INPUTS + sh].getVoltage();// sample and hold input
						else
							heldOuts[sh] = noises[sh];// sample and hold noise
					}
					// else no rising edge, so simply preserve heldOuts[sh], nothing to do
				}
				else { // no trig connected
					if (inputs[IN_INPUTS + sh].isConnected())
						heldOuts[sh] = inputs[IN_INPUTS + sh].getVoltage();// copy of input if no trig and input
					else
						heldOuts[sh] = noises[sh];// continuous noise if no trig and no input
				}
				outputs[OUT_OUTPUTS + sh].setVoltage(heldOuts[sh]);
			}
		}
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Lights
			for (int i = 0; i < 2; i++) {
				float red = trigBypass[i] ? 1.0f : 0.0f;
				float white = !trigBypass[i] ? trigLights[i] : 0.0f;
				lights[BYPASS_TRIG_LIGHTS + i * 2 + 0].value = white;
				lights[BYPASS_TRIG_LIGHTS + i * 2 + 1].value = red;
				lights[NOISE_RANGE_LIGHTS + i].value = noiseRange[i] ? 1.0f : 0.0f;
				trigLights[i] -= (trigLights[i] / lightLambda) * (float)args.sampleTime * displayRefreshStepSkips;
			}
			
		}// lightRefreshCounter
		
	}// step()
	
	float getNoise(int sh) {
		// some of the code in here is from Joel Robichaud - Nohmad Noise module
		float ret = 0.0f;
		int braneIndex = sh < 7 ? 0 : 1;
		int noiseIndex = noiseSources[sh];
		if (noiseIndex == WHITE) {
			ret = 5.0f * whiteNoise.white();
		}
		else if (noiseIndex == RED) {
			if (cacheHitRed[braneIndex])
				ret = -1.0 * cacheValRed[braneIndex];
			else {
				redFilter[braneIndex].process(whiteNoise.white());
				cacheValRed[braneIndex] = 5.0f * clamp(7.8f * redFilter[braneIndex].lowpass(), -1.0f, 1.0f);
				cacheHitRed[braneIndex] = true;
				ret = cacheValRed[braneIndex];
			}
		}
		else if (noiseIndex == PINK) {
			if (cacheHitPink[braneIndex])
				ret = -1.0 * cacheValPink[braneIndex];
			else {
				pinkFilter[braneIndex].process(whiteNoise.white());
				cacheValPink[braneIndex] = 5.0f * clamp(0.18f * pinkFilter[braneIndex].pink(), -1.0f, 1.0f);
				cacheHitPink[braneIndex] = true;
				ret = cacheValPink[braneIndex];
			}
		}
		else {// noiseIndex == BLUE
			if (cacheHitBlue[braneIndex])
				ret = -1.0 * cacheValBlue[braneIndex];
			else {
				pinkForBlueFilter[braneIndex].process(whiteNoise.white());
				blueFilter[braneIndex].process(pinkForBlueFilter[braneIndex].pink());
				cacheValBlue[braneIndex] = 5.0f * clamp(0.64f * blueFilter[braneIndex].highpass(), -1.0f, 1.0f);
				cacheHitBlue[braneIndex] = true;
				ret = cacheValBlue[braneIndex];
			}
		}
		
		// noise ranges
		if (noiseRange[0]) {
			if (sh >= 3 && sh <= 6)// 0 to 10 instead of -5 to 5
				ret += 5.0f;
		}
		if (noiseRange[1]) {
			if (sh >= 7 && sh <= 10) {// 0 to 1 instead of -5 to 5
				ret += 5.0f;
				ret *= 0.1f;
			}
			else if (sh >= 11) {// -1 to 1 instead of -5 to 5
				ret *= 0.2f;
			}	
		}
		return ret;
	}
};


struct BranesWidget : ModuleWidget {
	SvgPanel* lightPanel;
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Branes *module;
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
	}	
	
	BranesWidget(Branes *module) {
		setModule(module);

		// Main panels from Inkscape
        lightPanel = new SvgPanel();
        lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Branes-WL.svg")));
        box.size = lightPanel->box.size;
        addChild(lightPanel);
        darkPanel = new SvgPanel();
		darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Branes-DM.svg")));
		darkPanel->visible = false;
		addChild(darkPanel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		static constexpr float rowRulerHoldA = 132.5;
		static constexpr float rowRulerHoldB = 261.5f;
		static constexpr float radiusIn = 35.0f;
		static constexpr float radiusOut = 64.0f;
		static constexpr float offsetIn = 25.0f;
		static constexpr float offsetOut = 46.0f;
		
		
		// BraneA trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA), true, module, Branes::TRIG_INPUTS + 0, module ? &module->panelTheme : NULL));
		
		// BraneA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA + offsetIn), true, module, Branes::IN_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldA), true, module, Branes::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA - offsetIn), true, module, Branes::IN_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusIn), true, module, Branes::IN_INPUTS + 3, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA - offsetIn), true, module, Branes::IN_INPUTS + 4, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldA), true, module, Branes::IN_INPUTS + 5, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA + offsetIn), true, module, Branes::IN_INPUTS + 6, module ? &module->panelTheme : NULL));

		// BraneA outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA + offsetOut), false, module, Branes::OUT_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldA), false, module, Branes::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA - offsetOut), false, module, Branes::OUT_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusOut), false, module, Branes::OUT_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA - offsetOut), false, module, Branes::OUT_OUTPUTS + 4, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldA), false, module, Branes::OUT_OUTPUTS + 5, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA + offsetOut), false, module, Branes::OUT_OUTPUTS + 6, module ? &module->panelTheme : NULL));
		
		
		// BraneB trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB), true, module, Branes::TRIG_INPUTS + 1, module ? &module->panelTheme : NULL));
		
		// BraneB inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB - offsetIn), true, module, Branes::IN_INPUTS + 7, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldB), true, module, Branes::IN_INPUTS + 8, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB + offsetIn), true, module, Branes::IN_INPUTS + 9, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusIn), true, module, Branes::IN_INPUTS + 10, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB + offsetIn), true, module, Branes::IN_INPUTS + 11, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldB), true, module, Branes::IN_INPUTS + 12, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB - offsetIn), true, module, Branes::IN_INPUTS + 13, module ? &module->panelTheme : NULL));


		// BraneB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB - offsetOut), false, module, Branes::OUT_OUTPUTS + 7, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldB), false, module, Branes::OUT_OUTPUTS + 8, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB + offsetOut), false, module, Branes::OUT_OUTPUTS + 9, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusOut), false, module, Branes::OUT_OUTPUTS + 10, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB + offsetOut), false, module, Branes::OUT_OUTPUTS + 11, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldB), false, module, Branes::OUT_OUTPUTS + 12, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB - offsetOut), false, module, Branes::OUT_OUTPUTS + 13, module ? &module->panelTheme : NULL));
		
		
		// Trigger bypass (aka Vibrations)
		// Bypass buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 40.0f, 380.0f - 334.5f), module, Branes::TRIG_BYPASS_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 40.0f, 380.0f - 31.5f), module, Branes::TRIG_BYPASS_PARAMS + 1, module ? &module->panelTheme : NULL));
		// Bypass cv inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 68.0f, 380.0f - 315.5f), true, module, Branes::TRIG_BYPASS_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 68.0f, 380.0f - 50.5f), true, module, Branes::TRIG_BYPASS_INPUTS + 1, module ? &module->panelTheme : NULL));
		// Bypass LEDs near buttons
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter + 53.0f, 380.0f - 327.5f), module, Branes::BYPASS_TRIG_LIGHTS + 0 * 2));
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter + 53.0f, 380.0f - 38.5f), module, Branes::BYPASS_TRIG_LIGHTS + 1 * 2));
				
		// Bypass LEDs in branes
		// addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter + 5.5f, rowRulerHoldA + 19.5f), module, Branes::BYPASS_TRIG_LIGHTS + 0 * 2));
		// addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter - 5.5f, rowRulerHoldB - 19.5f), module, Branes::BYPASS_TRIG_LIGHTS + 1 * 2));
		
		// Noise range
		// Range buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 40.0f, 380.0f - 334.5f), module, Branes::NOISE_RANGE_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 40.0f, 380.0f - 31.5f), module, Branes::NOISE_RANGE_PARAMS + 1, module ? &module->panelTheme : NULL));
		// Range cv inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 68.0f, 380.0f - 315.5f), true, module, Branes::NOISE_RANGE_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 68.0f, 380.0f - 50.5f), true, module, Branes::NOISE_RANGE_INPUTS + 1, module ? &module->panelTheme : NULL));
		// Range LEDs near buttons
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 53.0f, 380.0f - 327.5f), module, Branes::NOISE_RANGE_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 53.0f, 380.0f - 38.5f), module, Branes::NOISE_RANGE_LIGHTS + 1));

	}
	
	void step() override {
		if (module) {
			lightPanel->visible = ((((Branes*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Branes*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelBranes = createModel<Branes, BranesWidget>("Branes");

/*CHANGE LOG


*/