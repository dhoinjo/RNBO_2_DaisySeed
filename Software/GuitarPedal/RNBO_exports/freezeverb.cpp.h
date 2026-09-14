/*******************************************************************************************************************
Copyright (c) 2023 Cycling '74

The code that Max generates automatically and that end users are capable of
exporting and using, and any associated documentation files (the “Software”)
is a work of authorship for which Cycling '74 is the author and owner for
copyright purposes.

This Software is dual-licensed either under the terms of the Cycling '74
License for Max-Generated Code for Export, or alternatively under the terms
of the General Public License (GPL) Version 3. You may use the Software
according to either of these licenses as it is most appropriate for your
project on a case-by-case basis (proprietary or not).

A) Cycling '74 License for Max-Generated Code for Export

A license is hereby granted, free of charge, to any person obtaining a copy
of the Software (“Licensee”) to use, copy, modify, merge, publish, and
distribute copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The Software is licensed to Licensee for all uses that do not include the sale,
sublicensing, or commercial distribution of software that incorporates this
source code. This means that the Licensee is free to use this software for
educational, research, and prototyping purposes, to create musical or other
creative works with software that incorporates this source code, or any other
use that does not constitute selling software that makes use of this source
code. Commercial distribution also includes the packaging of free software with
other paid software, hardware, or software-provided commercial services.

For entities with UNDER $200k in annual revenue or funding, a license is hereby
granted, free of charge, for the sale, sublicensing, or commercial distribution
of software that incorporates this source code, for as long as the entity's
annual revenue remains below $200k annual revenue or funding.

For entities with OVER $200k in annual revenue or funding interested in the
sale, sublicensing, or commercial distribution of software that incorporates
this source code, please send inquiries to licensing@cycling74.com.

The above copyright notice and this license shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Please see
https://support.cycling74.com/hc/en-us/articles/10730637742483-RNBO-Export-Licensing-FAQ
for additional information

B) General Public License Version 3 (GPLv3)
Details of the GPLv3 license can be found at: https://www.gnu.org/licenses/gpl-3.0.html
*******************************************************************************************************************/

#ifdef RNBO_LIB_PREFIX
#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#define RNBO_LIB_INCLUDE(X) STR(RNBO_LIB_PREFIX/X)
#else
#define RNBO_LIB_INCLUDE(X) #X
#endif // RNBO_LIB_PREFIX
#ifdef RNBO_INJECTPLATFORM
#define RNBO_USECUSTOMPLATFORM
#include RNBO_INJECTPLATFORM
#endif // RNBO_INJECTPLATFORM

#include RNBO_LIB_INCLUDE(RNBO_Common.h)
#include RNBO_LIB_INCLUDE(RNBO_AudioSignal.h)

namespace RNBO {


#define trunc(x) ((Int)(x))
#define autoref auto&

#if defined(__GNUC__) || defined(__clang__)
    #define RNBO_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RNBO_RESTRICT __restrict
#endif

#define FIXEDSIZEARRAYINIT(...) { }

template <class ENGINE = INTERNALENGINE> class FreezeVerb : public PatcherInterfaceImpl {

friend class EngineCore;
friend class Engine;
friend class MinimalEngine<>;
public:

FreezeVerb()
: _internalEngine(this)
{
}

~FreezeVerb()
{
    deallocateSignals();
}

Index getNumMidiInputPorts() const {
    return 0;
}

void processMidiEvent(MillisecondTime , int , ConstByteArray , Index ) {}

Index getNumMidiOutputPorts() const {
    return 0;
}

void process(
    const SampleValue * const* inputs,
    Index numInputs,
    SampleValue * const* outputs,
    Index numOutputs,
    Index n
) {
    this->vs = n;
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr, true);
    SampleValue * out1 = (numOutputs >= 1 && outputs[0] ? outputs[0] : this->dummyBuffer);
    SampleValue * out2 = (numOutputs >= 2 && outputs[1] ? outputs[1] : this->dummyBuffer);
    const SampleValue * in1 = (numInputs >= 1 && inputs[0] ? inputs[0] : this->zeroBuffer);
    const SampleValue * in2 = (numInputs >= 2 && inputs[1] ? inputs[1] : this->zeroBuffer);
    this->linetilde_01_perform(this->signals[0], n);
    this->linetilde_02_perform(this->signals[1], n);

    this->gen_01_perform(
        in1,
        in2,
        this->gen_01_amount,
        this->gen_01_input_gain,
        this->gen_01_reverb_time,
        this->gen_01_diffusion,
        this->gen_01_lp,
        this->gen_01_chorus,
        this->gen_01_flutter_speed,
        this->gen_01_degradation_amount,
        this->gen_01_degradation_speed,
        this->signals[2],
        this->signals[3],
        n
    );

    this->dspexpr_01_perform(this->signals[2], this->signals[0], out1, n);
    this->dspexpr_02_perform(this->signals[3], this->signals[1], out2, n);
    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->advanceTime((ENGINE*)nullptr);
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    RNBO_ASSERT(this->_isInitialized);

    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        Index i;

        for (i = 0; i < 4; i++) {
            this->signals[i] = resizeSignal(this->signals[i], this->maxvs, maxBlockSize);
        }

        this->globaltransport_tempo = resizeSignal(this->globaltransport_tempo, this->maxvs, maxBlockSize);
        this->globaltransport_state = resizeSignal(this->globaltransport_state, this->maxvs, maxBlockSize);
        this->zeroBuffer = resizeSignal(this->zeroBuffer, this->maxvs, maxBlockSize);
        this->dummyBuffer = resizeSignal(this->dummyBuffer, this->maxvs, maxBlockSize);
        this->didAllocateSignals = true;
    }

    RNBO_ASSERT(sampleRate == 48000);
    sampleRate = 48000;
    const bool sampleRateChanged = sampleRate != this->sr;
    const bool maxvsChanged = maxBlockSize != this->maxvs;
    const bool forceDSPSetup = sampleRateChanged || maxvsChanged || force;

    if (sampleRateChanged || maxvsChanged) {
        this->vs = maxBlockSize;
        this->maxvs = maxBlockSize;
        this->sr = sampleRate;
        this->invsr = 1 / sampleRate;
    }

    this->gen_01_dspsetup(forceDSPSetup);
    this->globaltransport_dspsetup(forceDSPSetup);

    if (sampleRateChanged)
        this->onSampleRateChanged(sampleRate);
}

number msToSamps(MillisecondTime ms, number sampleRate) {
    return ms * sampleRate * 0.001;
}

MillisecondTime sampsToMs(SampleIndex samps) {
    return samps * (this->invsr * 1000);
}

Index getNumInputChannels() const {
    return 2;
}

Index getNumOutputChannels() const {
    return 2;
}

DataRef* getDataRef(DataRefIndex index)  {
    switch (index) {
    case 0:
        {
        return addressOf(this->gen_01_ap1_bufferobj);
        break;
        }
    case 1:
        {
        return addressOf(this->gen_01_ap2_bufferobj);
        break;
        }
    case 2:
        {
        return addressOf(this->gen_01_ap3_bufferobj);
        break;
        }
    case 3:
        {
        return addressOf(this->gen_01_ap4_bufferobj);
        break;
        }
    case 4:
        {
        return addressOf(this->gen_01_dap1a_bufferobj);
        break;
        }
    case 5:
        {
        return addressOf(this->gen_01_dap1b_bufferobj);
        break;
        }
    case 6:
        {
        return addressOf(this->gen_01_del1_bufferobj);
        break;
        }
    case 7:
        {
        return addressOf(this->gen_01_dap2a_bufferobj);
        break;
        }
    case 8:
        {
        return addressOf(this->gen_01_dap2b_bufferobj);
        break;
        }
    case 9:
        {
        return addressOf(this->gen_01_del2_bufferobj);
        break;
        }
    default:
        {
        return nullptr;
        }
    }
}

DataRefIndex getNumDataRefs() const {
    return 10;
}

void processDataViewUpdate(DataRefIndex index, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (index == 0) {
        this->gen_01_ap1_buffer = reInitDataView(this->gen_01_ap1_buffer, this->gen_01_ap1_bufferobj);
    }

    if (index == 1) {
        this->gen_01_ap2_buffer = reInitDataView(this->gen_01_ap2_buffer, this->gen_01_ap2_bufferobj);
    }

    if (index == 2) {
        this->gen_01_ap3_buffer = reInitDataView(this->gen_01_ap3_buffer, this->gen_01_ap3_bufferobj);
    }

    if (index == 3) {
        this->gen_01_ap4_buffer = reInitDataView(this->gen_01_ap4_buffer, this->gen_01_ap4_bufferobj);
    }

    if (index == 4) {
        this->gen_01_dap1a_buffer = reInitDataView(this->gen_01_dap1a_buffer, this->gen_01_dap1a_bufferobj);
    }

    if (index == 5) {
        this->gen_01_dap1b_buffer = reInitDataView(this->gen_01_dap1b_buffer, this->gen_01_dap1b_bufferobj);
    }

    if (index == 6) {
        this->gen_01_del1_buffer = reInitDataView(this->gen_01_del1_buffer, this->gen_01_del1_bufferobj);
    }

    if (index == 7) {
        this->gen_01_dap2a_buffer = reInitDataView(this->gen_01_dap2a_buffer, this->gen_01_dap2a_bufferobj);
    }

    if (index == 8) {
        this->gen_01_dap2b_buffer = reInitDataView(this->gen_01_dap2b_buffer, this->gen_01_dap2b_bufferobj);
    }

    if (index == 9) {
        this->gen_01_del2_buffer = reInitDataView(this->gen_01_del2_buffer, this->gen_01_del2_bufferobj);
    }
}

void initialize() {
    RNBO_ASSERT(!this->_isInitialized);

    this->gen_01_ap1_bufferobj = initDataRef(
        this->gen_01_ap1_bufferobj,
        this->dataRefStrings->name0,
        true,
        this->dataRefStrings->file0,
        this->dataRefStrings->tag0
    );

    this->gen_01_ap2_bufferobj = initDataRef(
        this->gen_01_ap2_bufferobj,
        this->dataRefStrings->name1,
        true,
        this->dataRefStrings->file1,
        this->dataRefStrings->tag1
    );

    this->gen_01_ap3_bufferobj = initDataRef(
        this->gen_01_ap3_bufferobj,
        this->dataRefStrings->name2,
        true,
        this->dataRefStrings->file2,
        this->dataRefStrings->tag2
    );

    this->gen_01_ap4_bufferobj = initDataRef(
        this->gen_01_ap4_bufferobj,
        this->dataRefStrings->name3,
        true,
        this->dataRefStrings->file3,
        this->dataRefStrings->tag3
    );

    this->gen_01_dap1a_bufferobj = initDataRef(
        this->gen_01_dap1a_bufferobj,
        this->dataRefStrings->name4,
        true,
        this->dataRefStrings->file4,
        this->dataRefStrings->tag4
    );

    this->gen_01_dap1b_bufferobj = initDataRef(
        this->gen_01_dap1b_bufferobj,
        this->dataRefStrings->name5,
        true,
        this->dataRefStrings->file5,
        this->dataRefStrings->tag5
    );

    this->gen_01_del1_bufferobj = initDataRef(
        this->gen_01_del1_bufferobj,
        this->dataRefStrings->name6,
        true,
        this->dataRefStrings->file6,
        this->dataRefStrings->tag6
    );

    this->gen_01_dap2a_bufferobj = initDataRef(
        this->gen_01_dap2a_bufferobj,
        this->dataRefStrings->name7,
        true,
        this->dataRefStrings->file7,
        this->dataRefStrings->tag7
    );

    this->gen_01_dap2b_bufferobj = initDataRef(
        this->gen_01_dap2b_bufferobj,
        this->dataRefStrings->name8,
        true,
        this->dataRefStrings->file8,
        this->dataRefStrings->tag8
    );

    this->gen_01_del2_bufferobj = initDataRef(
        this->gen_01_del2_bufferobj,
        this->dataRefStrings->name9,
        true,
        this->dataRefStrings->file9,
        this->dataRefStrings->tag9
    );

    this->assign_defaults();
    this->applyState();
    this->gen_01_ap1_bufferobj->setIndex(0);
    this->gen_01_ap1_buffer = new Float64Buffer(this->gen_01_ap1_bufferobj);
    this->gen_01_ap2_bufferobj->setIndex(1);
    this->gen_01_ap2_buffer = new Float64Buffer(this->gen_01_ap2_bufferobj);
    this->gen_01_ap3_bufferobj->setIndex(2);
    this->gen_01_ap3_buffer = new Float64Buffer(this->gen_01_ap3_bufferobj);
    this->gen_01_ap4_bufferobj->setIndex(3);
    this->gen_01_ap4_buffer = new Float64Buffer(this->gen_01_ap4_bufferobj);
    this->gen_01_dap1a_bufferobj->setIndex(4);
    this->gen_01_dap1a_buffer = new Float64Buffer(this->gen_01_dap1a_bufferobj);
    this->gen_01_dap1b_bufferobj->setIndex(5);
    this->gen_01_dap1b_buffer = new Float64Buffer(this->gen_01_dap1b_bufferobj);
    this->gen_01_del1_bufferobj->setIndex(6);
    this->gen_01_del1_buffer = new Float64Buffer(this->gen_01_del1_bufferobj);
    this->gen_01_dap2a_bufferobj->setIndex(7);
    this->gen_01_dap2a_buffer = new Float64Buffer(this->gen_01_dap2a_bufferobj);
    this->gen_01_dap2b_bufferobj->setIndex(8);
    this->gen_01_dap2b_buffer = new Float64Buffer(this->gen_01_dap2b_bufferobj);
    this->gen_01_del2_bufferobj->setIndex(9);
    this->gen_01_del2_buffer = new Float64Buffer(this->gen_01_del2_bufferobj);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
    this->_isInitialized = true;
}

void setParameterValue(ParameterIndex index, ParameterValue v, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    switch (index) {
    case 0:
        {
        this->param_01_value_set(v);
        break;
        }
    case 1:
        {
        this->param_02_value_set(v);
        break;
        }
    case 2:
        {
        this->param_03_value_set(v);
        break;
        }
    case 3:
        {
        this->param_04_value_set(v);
        break;
        }
    case 4:
        {
        this->param_05_value_set(v);
        break;
        }
    case 5:
        {
        this->param_06_value_set(v);
        break;
        }
    case 6:
        {
        this->param_07_value_set(v);
        break;
        }
    }
}

void processParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValue(index, value, time);
}

void processParameterBangEvent(ParameterIndex index, MillisecondTime time) {
    this->setParameterValue(index, this->getParameterValue(index), time);
}

void processNormalizedParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValueNormalized(index, value, time);
}

ParameterValue getParameterValue(ParameterIndex index)  {
    switch (index) {
    case 0:
        {
        return this->param_01_value;
        }
    case 1:
        {
        return this->param_02_value;
        }
    case 2:
        {
        return this->param_03_value;
        }
    case 3:
        {
        return this->param_04_value;
        }
    case 4:
        {
        return this->param_05_value;
        }
    case 5:
        {
        return this->param_06_value;
        }
    case 6:
        {
        return this->param_07_value;
        }
    default:
        {
        return 0;
        }
    }
}

ParameterIndex getNumSignalInParameters() const {
    return 0;
}

ParameterIndex getNumSignalOutParameters() const {
    return 0;
}

ParameterIndex getNumParameters() const {
    return 7;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "fsw1";
        }
    case 1:
        {
        return "led1";
        }
    case 2:
        {
        return "fsw2";
        }
    case 3:
        {
        return "led2";
        }
    case 4:
        {
        return "amount";
        }
    case 5:
        {
        return "lp";
        }
    case 6:
        {
        return "reverb_time";
        }
    default:
        {
        return "bogus";
        }
    }
}

ConstCharPointer getParameterId(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "fsw1";
        }
    case 1:
        {
        return "led1";
        }
    case 2:
        {
        return "fsw2";
        }
    case 3:
        {
        return "led2";
        }
    case 4:
        {
        return "amount";
        }
    case 5:
        {
        return "lp";
        }
    case 6:
        {
        return "reverb_time";
        }
    default:
        {
        return "bogus";
        }
    }
}

void getParameterInfo(ParameterIndex index, ParameterInfo * info) const {
    {
        switch (index) {
        case 0:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 1:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 2:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 3:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 4:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 5:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 6:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        }
    }
}

ParameterValue applyStepsToNormalizedParameterValue(ParameterValue normalizedValue, int steps) const {
    if (steps == 1) {
        if (normalizedValue > 0) {
            normalizedValue = 1.;
        }
    } else {
        ParameterValue oneStep = (number)1. / (steps - 1);
        ParameterValue numberOfSteps = rnbo_fround(normalizedValue / oneStep * 1 / (number)1) * (number)1;
        normalizedValue = numberOfSteps * oneStep;
    }

    return normalizedValue;
}

ParameterValue convertToNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        {
        {
            value = (value < 0 ? 0 : (value > 1 ? 1 : value));
            ParameterValue normalizedValue = (value - 0) / (1 - 0);
            return normalizedValue;
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    value = (value < 0 ? 0 : (value > 1 ? 1 : value));

    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        {
        {
            {
                return 0 + value * (1 - 0);
            }
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue constrainParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
        {
        return this->param_01_value_constrain(value);
        }
    case 1:
        {
        return this->param_02_value_constrain(value);
        }
    case 2:
        {
        return this->param_03_value_constrain(value);
        }
    case 3:
        {
        return this->param_04_value_constrain(value);
        }
    case 4:
        {
        return this->param_05_value_constrain(value);
        }
    case 5:
        {
        return this->param_06_value_constrain(value);
        }
    case 6:
        {
        return this->param_07_value_constrain(value);
        }
    default:
        {
        return value;
        }
    }
}

void processNumMessage(MessageTag tag, MessageTag objectId, MillisecondTime time, number payload) {
    this->updateTime(time, (ENGINE*)nullptr);

    switch (tag) {
    case TAG("valin"):
        {
        if (TAG("toggle_obj-49") == objectId)
            this->toggle_01_valin_set(payload);

        if (TAG("toggle_obj-50") == objectId)
            this->toggle_02_valin_set(payload);

        break;
        }
    }
}

void processListMessage(MessageTag , MessageTag , MillisecondTime , const list& ) {}

void processBangMessage(MessageTag , MessageTag , MillisecondTime ) {}

MessageTagInfo resolveTag(MessageTag tag) const {
    switch (tag) {
    case TAG("valout"):
        {
        return "valout";
        }
    case TAG("toggle_obj-49"):
        {
        return "toggle_obj-49";
        }
    case TAG("toggle_obj-50"):
        {
        return "toggle_obj-50";
        }
    case TAG("valin"):
        {
        return "valin";
        }
    }

    return "";
}

MessageIndex getNumMessages() const {
    return 0;
}

const MessageInfo& getMessageInfo(MessageIndex index) const {
    switch (index) {

    }

    return NullMessageInfo;
}

protected:

		
void advanceTime(EXTERNALENGINE*) {}
void advanceTime(INTERNALENGINE*) {
	_internalEngine.advanceTime(sampstoms(this->vs));
}

void processInternalEvents(MillisecondTime time) {
	_internalEngine.processEventsUntil(time);
}

void updateTime(MillisecondTime time, INTERNALENGINE*, bool inProcess = false) {
	if (time == TimeNow) time = getPatcherTime();
	processInternalEvents(inProcess ? time + sampsToMs(this->vs) : time);
	updateTime(time, (EXTERNALENGINE*)nullptr);
}

FreezeVerb* operator->() {
    return this;
}
const FreezeVerb* operator->() const {
    return this;
}
FreezeVerb* getTopLevelPatcher() {
    return this;
}

void cancelClockEvents()
{
    getEngine()->flushClockEvents(this, -1035241583, false);
    getEngine()->flushClockEvents(this, 1700604240, false);
}

template<typename LISTTYPE = list> void listquicksort(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    if (l < h) {
        Int p = (Int)(this->listpartition(arr, sortindices, l, h, ascending));
        this->listquicksort(arr, sortindices, l, p - 1, ascending);
        this->listquicksort(arr, sortindices, p + 1, h, ascending);
    }
}

template<typename LISTTYPE = list> Int listpartition(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    number x = arr[(Index)h];
    Int i = (Int)(l - 1);

    for (Int j = (Int)(l); j <= h - 1; j++) {
        bool asc = (bool)((bool)(ascending) && arr[(Index)j] <= x);
        bool desc = (bool)((bool)(!(bool)(ascending)) && arr[(Index)j] >= x);

        if ((bool)(asc) || (bool)(desc)) {
            i++;
            this->listswapelements(arr, i, j);
            this->listswapelements(sortindices, i, j);
        }
    }

    i++;
    this->listswapelements(arr, i, h);
    this->listswapelements(sortindices, i, h);
    return i;
}

template<typename LISTTYPE = list> void listswapelements(LISTTYPE& arr, Int a, Int b) {
    auto tmp = arr[(Index)a];
    arr[(Index)a] = arr[(Index)b];
    arr[(Index)b] = tmp;
}

UInt64 currentsampletime() {
    return this->audioProcessSampleCount + this->sampleOffsetIntoNextAudioBuffer;
}

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

inline number linearinterp(number frac, number x, number y) {
    return x + (y - x) * frac;
}

inline number cubicinterp(number a, number w, number x, number y, number z) {
    number a1 = 1. + a;
    number aa = a * a1;
    number b = 1. - a;
    number b1 = 2. - a;
    number bb = b * b1;
    number fw = -.1666667 * bb * a;
    number fx = .5 * bb * a1;
    number fy = .5 * aa * b1;
    number fz = -.1666667 * aa * b;
    return w * fw + x * fx + y * fy + z * fz;
}

inline number fastcubicinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = z - y - w + x;
    number f1 = w - x - f0;
    number f2 = y - w;
    number f3 = x;
    return f0 * a * a2 + f1 * a2 + f2 * a + f3;
}

inline number splineinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = -0.5 * w + 1.5 * x - 1.5 * y + 0.5 * z;
    number f1 = w - 2.5 * x + 2 * y - 0.5 * z;
    number f2 = -0.5 * w + 0.5 * y;
    return f0 * a * a2 + f1 * a2 + f2 * a + x;
}

inline number spline6interp(number a, number y0, number y1, number y2, number y3, number y4, number y5) {
    number ym2py2 = y0 + y4;
    number ym1py1 = y1 + y3;
    number y2mym2 = y4 - y0;
    number y1mym1 = y3 - y1;
    number sixthym1py1 = (number)1 / (number)6.0 * ym1py1;
    number c0 = (number)1 / (number)120.0 * ym2py2 + (number)13 / (number)60.0 * ym1py1 + (number)11 / (number)20.0 * y2;
    number c1 = (number)1 / (number)24.0 * y2mym2 + (number)5 / (number)12.0 * y1mym1;
    number c2 = (number)1 / (number)12.0 * ym2py2 + sixthym1py1 - (number)1 / (number)2.0 * y2;
    number c3 = (number)1 / (number)12.0 * y2mym2 - (number)1 / (number)6.0 * y1mym1;
    number c4 = (number)1 / (number)24.0 * ym2py2 - sixthym1py1 + (number)1 / (number)4.0 * y2;
    number c5 = (number)1 / (number)120.0 * (y5 - y0) + (number)1 / (number)24.0 * (y1 - y4) + (number)1 / (number)12.0 * (y3 - y2);
    return ((((c5 * a + c4) * a + c3) * a + c2) * a + c1) * a + c0;
}

inline number cosT8(number r) {
    number t84 = 56.0;
    number t83 = 1680.0;
    number t82 = 20160.0;
    number t81 = 2.4801587302e-05;
    number t73 = 42.0;
    number t72 = 840.0;
    number t71 = 1.9841269841e-04;

    if (r < 0.785398163397448309615660845819875721 && r > -0.785398163397448309615660845819875721) {
        number rr = r * r;
        return 1.0 - rr * t81 * (t82 - rr * (t83 - rr * (t84 - rr)));
    } else if (r > 0.0) {
        r -= 1.57079632679489661923132169163975144;
        number rr = r * r;
        return -r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    } else {
        r += 1.57079632679489661923132169163975144;
        number rr = r * r;
        return r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    }
}

inline number cosineinterp(number frac, number x, number y) {
    number a2 = (1.0 - this->cosT8(frac * 3.14159265358979323846)) / (number)2.0;
    return x * (1.0 - a2) + y * a2;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
}

Index voice() {
    return this->_voiceIndex;
}

number random(number low, number high) {
    number range = high - low;
    return globalrandom() * range + low;
}

number wrap(number x, number low, number high) {
    number lo;
    number hi;

    if (low == high)
        return low;

    if (low > high) {
        hi = low;
        lo = high;
    } else {
        lo = low;
        hi = high;
    }

    number range = hi - lo;

    if (x >= lo && x < hi)
        return x;

    if (range <= 0.000000001)
        return lo;

    Int numWraps = (Int)(trunc((x - lo) / range));
    numWraps = numWraps - ((x < lo ? 1 : 0));
    number result = x - range * numWraps;

    if (result >= hi)
        return result - range;
    else
        return result;
}

MillisecondTime sampstoms(number samps) {
    return samps * 1000 / this->sr;
}

void param_01_value_set(number v) {
    v = this->param_01_value_constrain(v);
    this->param_01_value = v;
    this->sendParameter(0, false);

    if (this->param_01_value != this->param_01_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_01_lastValue = this->param_01_value;
    }

    this->delta_01_x_set(v);
}

void param_03_value_set(number v) {
    v = this->param_03_value_constrain(v);
    this->param_03_value = v;
    this->sendParameter(2, false);

    if (this->param_03_value != this->param_03_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_03_lastValue = this->param_03_value;
    }

    this->delta_02_x_set(v);
}

void param_05_value_set(number v) {
    v = this->param_05_value_constrain(v);
    this->param_05_value = v;
    this->sendParameter(4, false);

    if (this->param_05_value != this->param_05_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_05_lastValue = this->param_05_value;
    }

    this->gen_01_amount_set(v);
}

void param_06_value_set(number v) {
    v = this->param_06_value_constrain(v);
    this->param_06_value = v;
    this->sendParameter(5, false);

    if (this->param_06_value != this->param_06_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_06_lastValue = this->param_06_value;
    }

    this->gen_01_lp_set(v);
}

void param_07_value_set(number v) {
    v = this->param_07_value_constrain(v);
    this->param_07_value = v;
    this->sendParameter(6, false);

    if (this->param_07_value != this->param_07_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_07_lastValue = this->param_07_value;
    }

    this->gen_01_reverb_time_set(v);
}

MillisecondTime getPatcherTime() const {
    return this->_currentTime;
}

void toggle_01_valin_set(number v) {
    this->toggle_01_value_number_set(v);
}

void toggle_02_valin_set(number v) {
    this->toggle_02_value_number_set(v);
}

void linetilde_01_target_bang() {}

void linetilde_02_target_bang() {}

void deallocateSignals() {
    Index i;

    for (i = 0; i < 4; i++) {
        this->signals[i] = freeSignal(this->signals[i]);
    }

    this->globaltransport_tempo = freeSignal(this->globaltransport_tempo);
    this->globaltransport_state = freeSignal(this->globaltransport_state);
    this->zeroBuffer = freeSignal(this->zeroBuffer);
    this->dummyBuffer = freeSignal(this->dummyBuffer);
}

Index getMaxBlockSize() const {
    return this->maxvs;
}

number getSampleRate() const {
    return 48000;
}

bool hasFixedVectorSize() const {
    return false;
}

void setProbingTarget(MessageTag ) {}

void fillDataRef(DataRefIndex , DataRef& ) {}

void zeroDataRef(DataRef& ref) {
    ref->setZero();
}

void allocateDataRefs() {
    this->gen_01_ap1_buffer = this->gen_01_ap1_buffer->allocateIfNeeded();

    if (this->gen_01_ap1_bufferobj->hasRequestedSize()) {
        if (this->gen_01_ap1_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_ap1_bufferobj);

        this->getEngine()->sendDataRefUpdated(0);
    }

    this->gen_01_ap2_buffer = this->gen_01_ap2_buffer->allocateIfNeeded();

    if (this->gen_01_ap2_bufferobj->hasRequestedSize()) {
        if (this->gen_01_ap2_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_ap2_bufferobj);

        this->getEngine()->sendDataRefUpdated(1);
    }

    this->gen_01_ap3_buffer = this->gen_01_ap3_buffer->allocateIfNeeded();

    if (this->gen_01_ap3_bufferobj->hasRequestedSize()) {
        if (this->gen_01_ap3_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_ap3_bufferobj);

        this->getEngine()->sendDataRefUpdated(2);
    }

    this->gen_01_ap4_buffer = this->gen_01_ap4_buffer->allocateIfNeeded();

    if (this->gen_01_ap4_bufferobj->hasRequestedSize()) {
        if (this->gen_01_ap4_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_ap4_bufferobj);

        this->getEngine()->sendDataRefUpdated(3);
    }

    this->gen_01_dap1a_buffer = this->gen_01_dap1a_buffer->allocateIfNeeded();

    if (this->gen_01_dap1a_bufferobj->hasRequestedSize()) {
        if (this->gen_01_dap1a_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_dap1a_bufferobj);

        this->getEngine()->sendDataRefUpdated(4);
    }

    this->gen_01_dap1b_buffer = this->gen_01_dap1b_buffer->allocateIfNeeded();

    if (this->gen_01_dap1b_bufferobj->hasRequestedSize()) {
        if (this->gen_01_dap1b_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_dap1b_bufferobj);

        this->getEngine()->sendDataRefUpdated(5);
    }

    this->gen_01_del1_buffer = this->gen_01_del1_buffer->allocateIfNeeded();

    if (this->gen_01_del1_bufferobj->hasRequestedSize()) {
        if (this->gen_01_del1_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_del1_bufferobj);

        this->getEngine()->sendDataRefUpdated(6);
    }

    this->gen_01_dap2a_buffer = this->gen_01_dap2a_buffer->allocateIfNeeded();

    if (this->gen_01_dap2a_bufferobj->hasRequestedSize()) {
        if (this->gen_01_dap2a_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_dap2a_bufferobj);

        this->getEngine()->sendDataRefUpdated(7);
    }

    this->gen_01_dap2b_buffer = this->gen_01_dap2b_buffer->allocateIfNeeded();

    if (this->gen_01_dap2b_bufferobj->hasRequestedSize()) {
        if (this->gen_01_dap2b_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_dap2b_bufferobj);

        this->getEngine()->sendDataRefUpdated(8);
    }

    this->gen_01_del2_buffer = this->gen_01_del2_buffer->allocateIfNeeded();

    if (this->gen_01_del2_bufferobj->hasRequestedSize()) {
        if (this->gen_01_del2_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_del2_bufferobj);

        this->getEngine()->sendDataRefUpdated(9);
    }
}

void initializeObjects() {
    this->gen_01_ap1_init();
    this->gen_01_ap2_init();
    this->gen_01_ap3_init();
    this->gen_01_ap4_init();
    this->gen_01_dap1a_init();
    this->gen_01_dap1b_init();
    this->gen_01_del1_init();
    this->gen_01_dap2a_init();
    this->gen_01_dap2b_init();
    this->gen_01_del2_init();
    this->gen_01_lp_decay_1_init();
    this->gen_01_lp_decay_2_init();
    this->gen_01_lfo1_phase_init();
    this->gen_01_lfo2_phase_init();
    this->gen_01_noise_state_3_init();
    this->gen_01_noise_state_4_init();
    this->gen_01_noise_6_init();
    this->gen_01_noise_8_init();
}

Index getIsMuted()  {
    return this->isMuted;
}

void setIsMuted(Index v)  {
    this->isMuted = v;
}

void onSampleRateChanged(double ) {}

void extractState(PatcherStateInterface& ) {}

void applyState() {}

void processClockEvent(MillisecondTime time, ClockId index, bool hasValue, ParameterValue value) {
    RNBO_UNUSED(value);
    RNBO_UNUSED(hasValue);
    this->updateTime(time, (ENGINE*)nullptr);

    switch (index) {
    case -1035241583:
        {
        this->linetilde_01_target_bang();
        break;
        }
    case 1700604240:
        {
        this->linetilde_02_target_bang();
        break;
        }
    }
}

void processOutletAtCurrentTime(EngineLink* , OutletIndex , ParameterValue ) {}

void processOutletEvent(
    EngineLink* sender,
    OutletIndex index,
    ParameterValue value,
    MillisecondTime time
) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->processOutletAtCurrentTime(sender, index, value);
}

void sendOutlet(OutletIndex index, ParameterValue value) {
    this->getEngine()->sendOutlet(this, index, value);
}

void startup() {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);

    {
        this->scheduleParamInit(0, 0);
    }

    {
        this->scheduleParamInit(1, 0);
    }

    {
        this->scheduleParamInit(2, 0);
    }

    {
        this->scheduleParamInit(3, 0);
    }

    {
        this->scheduleParamInit(4, 2);
    }

    {
        this->scheduleParamInit(5, 3);
    }

    {
        this->scheduleParamInit(6, 1);
    }

    this->processParamInitEvents();
}

number param_01_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number param_02_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

void param_02_value_set(number v) {
    v = this->param_02_value_constrain(v);
    this->param_02_value = v;
    this->sendParameter(1, false);

    if (this->param_02_value != this->param_02_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_02_lastValue = this->param_02_value;
    }
}

void pack_01_in2_set(number v) {
    this->pack_01_data[1] = v;
}

void trigger_01_out2_set(number v) {
    this->pack_01_in2_set(v);
}

void linetilde_01_time_set(number v) {
    this->linetilde_01_time = v;
}

template<typename LISTTYPE> void linetilde_01_segments_set(const LISTTYPE& v) {
    this->linetilde_01_segments = jsCreateListCopy(v);

    if ((bool)(v->length)) {
        if (v->length == 1 && this->linetilde_01_time == 0) {
            this->linetilde_01_activeRamps->length = 0;
            this->linetilde_01_currentValue = v[0];
        } else {
            auto currentTime = this->currentsampletime();
            number lastRampValue = this->linetilde_01_currentValue;
            number rampEnd = currentTime - this->sampleOffsetIntoNextAudioBuffer;

            for (Index i = 0; i < this->linetilde_01_activeRamps->length; i += 3) {
                rampEnd = this->linetilde_01_activeRamps[(Index)(i + 2)];

                if (rampEnd > currentTime) {
                    this->linetilde_01_activeRamps[(Index)(i + 2)] = currentTime;
                    number diff = rampEnd - currentTime;
                    number valueDiff = diff * this->linetilde_01_activeRamps[(Index)(i + 1)];
                    lastRampValue = this->linetilde_01_activeRamps[(Index)i] - valueDiff;
                    this->linetilde_01_activeRamps[(Index)i] = lastRampValue;
                    this->linetilde_01_activeRamps->length = i + 3;
                    rampEnd = currentTime;
                } else {
                    lastRampValue = this->linetilde_01_activeRamps[(Index)i];
                }
            }

            if (rampEnd < currentTime) {
                this->linetilde_01_activeRamps->push(lastRampValue);
                this->linetilde_01_activeRamps->push(0);
                this->linetilde_01_activeRamps->push(currentTime);
            }

            number lastRampEnd = currentTime;

            for (Index i = 0; i < v->length; i += 2) {
                number destinationValue = v[(Index)i];
                number inc = 0;
                number rampTimeInSamples;

                if (v->length > i + 1) {
                    rampTimeInSamples = this->mstosamps(v[(Index)(i + 1)]);

                    if ((bool)(this->linetilde_01_keepramp)) {
                        this->linetilde_01_time_set(v[(Index)(i + 1)]);
                    }
                } else {
                    rampTimeInSamples = this->mstosamps(this->linetilde_01_time);
                }

                if (rampTimeInSamples <= 0) {
                    rampTimeInSamples = 1;
                }

                inc = (destinationValue - lastRampValue) / rampTimeInSamples;
                lastRampEnd += rampTimeInSamples;
                this->linetilde_01_activeRamps->push(destinationValue);
                this->linetilde_01_activeRamps->push(inc);
                this->linetilde_01_activeRamps->push(lastRampEnd);
                lastRampValue = destinationValue;
            }
        }
    }
}

template<typename LISTTYPE> void pack_01_out_set(const LISTTYPE& v) {
    this->linetilde_01_segments_set(v);
}

void pack_01_in1_number_set(number v) {
    this->pack_01_data[0] = v;
    this->pack_01_out_set(this->pack_01_data);
}

void trigger_01_out1_set(number v) {
    this->pack_01_in1_number_set(v);
}

void trigger_01_input_number_set(number v) {
    this->trigger_01_out2_set(10);
    this->trigger_01_out1_set(v);
}

void toggle_01_output_set(number v) {
    this->param_02_value_set(v);
    this->trigger_01_input_number_set(v);
}

void toggle_01_value_number_set(number v) {
    this->toggle_01_value_number_setter(v);
    v = this->toggle_01_value_number;
    this->toggle_01_output_set(v);
}

void toggle_01_value_bang_bang() {
    number val = (this->toggle_01_value_number == 1 ? 0 : 1);
    this->toggle_01_value_number_set(val);
}

void select_01_match1_bang() {
    this->toggle_01_value_bang_bang();
}

void select_01_nomatch_number_set(number ) {}

void select_01_input_number_set(number v) {
    if (v == this->select_01_test1)
        this->select_01_match1_bang();
    else
        this->select_01_nomatch_number_set(v);
}

void delta_01_out1_set(number v) {
    this->select_01_input_number_set(v);
}

void delta_01_x_set(number x) {
    this->delta_01_x = x;
    number temp = (number)(x - this->delta_01_prev);
    this->delta_01_prev = x;

    {
        this->delta_01_out1_set(temp);
        return;
    }
}

number param_03_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number param_04_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

void param_04_value_set(number v) {
    v = this->param_04_value_constrain(v);
    this->param_04_value = v;
    this->sendParameter(3, false);

    if (this->param_04_value != this->param_04_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_04_lastValue = this->param_04_value;
    }
}

void pack_02_in2_set(number v) {
    this->pack_02_data[1] = v;
}

void trigger_02_out2_set(number v) {
    this->pack_02_in2_set(v);
}

void linetilde_02_time_set(number v) {
    this->linetilde_02_time = v;
}

template<typename LISTTYPE> void linetilde_02_segments_set(const LISTTYPE& v) {
    this->linetilde_02_segments = jsCreateListCopy(v);

    if ((bool)(v->length)) {
        if (v->length == 1 && this->linetilde_02_time == 0) {
            this->linetilde_02_activeRamps->length = 0;
            this->linetilde_02_currentValue = v[0];
        } else {
            auto currentTime = this->currentsampletime();
            number lastRampValue = this->linetilde_02_currentValue;
            number rampEnd = currentTime - this->sampleOffsetIntoNextAudioBuffer;

            for (Index i = 0; i < this->linetilde_02_activeRamps->length; i += 3) {
                rampEnd = this->linetilde_02_activeRamps[(Index)(i + 2)];

                if (rampEnd > currentTime) {
                    this->linetilde_02_activeRamps[(Index)(i + 2)] = currentTime;
                    number diff = rampEnd - currentTime;
                    number valueDiff = diff * this->linetilde_02_activeRamps[(Index)(i + 1)];
                    lastRampValue = this->linetilde_02_activeRamps[(Index)i] - valueDiff;
                    this->linetilde_02_activeRamps[(Index)i] = lastRampValue;
                    this->linetilde_02_activeRamps->length = i + 3;
                    rampEnd = currentTime;
                } else {
                    lastRampValue = this->linetilde_02_activeRamps[(Index)i];
                }
            }

            if (rampEnd < currentTime) {
                this->linetilde_02_activeRamps->push(lastRampValue);
                this->linetilde_02_activeRamps->push(0);
                this->linetilde_02_activeRamps->push(currentTime);
            }

            number lastRampEnd = currentTime;

            for (Index i = 0; i < v->length; i += 2) {
                number destinationValue = v[(Index)i];
                number inc = 0;
                number rampTimeInSamples;

                if (v->length > i + 1) {
                    rampTimeInSamples = this->mstosamps(v[(Index)(i + 1)]);

                    if ((bool)(this->linetilde_02_keepramp)) {
                        this->linetilde_02_time_set(v[(Index)(i + 1)]);
                    }
                } else {
                    rampTimeInSamples = this->mstosamps(this->linetilde_02_time);
                }

                if (rampTimeInSamples <= 0) {
                    rampTimeInSamples = 1;
                }

                inc = (destinationValue - lastRampValue) / rampTimeInSamples;
                lastRampEnd += rampTimeInSamples;
                this->linetilde_02_activeRamps->push(destinationValue);
                this->linetilde_02_activeRamps->push(inc);
                this->linetilde_02_activeRamps->push(lastRampEnd);
                lastRampValue = destinationValue;
            }
        }
    }
}

template<typename LISTTYPE> void pack_02_out_set(const LISTTYPE& v) {
    this->linetilde_02_segments_set(v);
}

void pack_02_in1_number_set(number v) {
    this->pack_02_data[0] = v;
    this->pack_02_out_set(this->pack_02_data);
}

void trigger_02_out1_set(number v) {
    this->pack_02_in1_number_set(v);
}

void trigger_02_input_number_set(number v) {
    this->trigger_02_out2_set(10);
    this->trigger_02_out1_set(v);
}

void toggle_02_output_set(number v) {
    this->param_04_value_set(v);
    this->trigger_02_input_number_set(v);
}

void toggle_02_value_number_set(number v) {
    this->toggle_02_value_number_setter(v);
    v = this->toggle_02_value_number;
    this->toggle_02_output_set(v);
}

void toggle_02_value_bang_bang() {
    number val = (this->toggle_02_value_number == 1 ? 0 : 1);
    this->toggle_02_value_number_set(val);
}

void select_02_match1_bang() {
    this->toggle_02_value_bang_bang();
}

void select_02_nomatch_number_set(number ) {}

void select_02_input_number_set(number v) {
    if (v == this->select_02_test1)
        this->select_02_match1_bang();
    else
        this->select_02_nomatch_number_set(v);
}

void delta_02_out1_set(number v) {
    this->select_02_input_number_set(v);
}

void delta_02_x_set(number x) {
    this->delta_02_x = x;
    number temp = (number)(x - this->delta_02_prev);
    this->delta_02_prev = x;

    {
        this->delta_02_out1_set(temp);
        return;
    }
}

number param_05_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_amount_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_amount_set(number v) {
    v = this->gen_01_amount_constrain(v);
    this->gen_01_amount = v;
}

number param_06_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_lp_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_lp_set(number v) {
    v = this->gen_01_lp_constrain(v);
    this->gen_01_lp = v;
}

number param_07_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_reverb_time_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_reverb_time_set(number v) {
    v = this->gen_01_reverb_time_constrain(v);
    this->gen_01_reverb_time = v;
}

void linetilde_01_perform(SampleValue * out, Index n) {
    auto __linetilde_01_time = this->linetilde_01_time;
    auto __linetilde_01_keepramp = this->linetilde_01_keepramp;
    auto __linetilde_01_currentValue = this->linetilde_01_currentValue;
    Index i = 0;

    if ((bool)(this->linetilde_01_activeRamps->length)) {
        while ((bool)(this->linetilde_01_activeRamps->length) && i < n) {
            number destinationValue = this->linetilde_01_activeRamps[0];
            number inc = this->linetilde_01_activeRamps[1];
            number rampTimeInSamples = this->linetilde_01_activeRamps[2] - this->audioProcessSampleCount - i;
            number val = __linetilde_01_currentValue;

            while (rampTimeInSamples > 0 && i < n) {
                out[(Index)i] = val;
                val += inc;
                i++;
                rampTimeInSamples--;
            }

            if (rampTimeInSamples <= 0) {
                val = destinationValue;
                this->linetilde_01_activeRamps->splice(0, 3);

                if ((bool)(!(bool)(this->linetilde_01_activeRamps->length))) {
                    this->getEngine()->scheduleClockEventWithValue(
                        this,
                        -1035241583,
                        this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                        0
                    );;

                    if ((bool)(!(bool)(__linetilde_01_keepramp))) {
                        __linetilde_01_time = 0;
                    }
                }
            }

            __linetilde_01_currentValue = val;
        }
    }

    while (i < n) {
        out[(Index)i] = __linetilde_01_currentValue;
        i++;
    }

    this->linetilde_01_currentValue = __linetilde_01_currentValue;
    this->linetilde_01_time = __linetilde_01_time;
}

void linetilde_02_perform(SampleValue * out, Index n) {
    auto __linetilde_02_time = this->linetilde_02_time;
    auto __linetilde_02_keepramp = this->linetilde_02_keepramp;
    auto __linetilde_02_currentValue = this->linetilde_02_currentValue;
    Index i = 0;

    if ((bool)(this->linetilde_02_activeRamps->length)) {
        while ((bool)(this->linetilde_02_activeRamps->length) && i < n) {
            number destinationValue = this->linetilde_02_activeRamps[0];
            number inc = this->linetilde_02_activeRamps[1];
            number rampTimeInSamples = this->linetilde_02_activeRamps[2] - this->audioProcessSampleCount - i;
            number val = __linetilde_02_currentValue;

            while (rampTimeInSamples > 0 && i < n) {
                out[(Index)i] = val;
                val += inc;
                i++;
                rampTimeInSamples--;
            }

            if (rampTimeInSamples <= 0) {
                val = destinationValue;
                this->linetilde_02_activeRamps->splice(0, 3);

                if ((bool)(!(bool)(this->linetilde_02_activeRamps->length))) {
                    this->getEngine()->scheduleClockEventWithValue(
                        this,
                        1700604240,
                        this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                        0
                    );;

                    if ((bool)(!(bool)(__linetilde_02_keepramp))) {
                        __linetilde_02_time = 0;
                    }
                }
            }

            __linetilde_02_currentValue = val;
        }
    }

    while (i < n) {
        out[(Index)i] = __linetilde_02_currentValue;
        i++;
    }

    this->linetilde_02_currentValue = __linetilde_02_currentValue;
    this->linetilde_02_time = __linetilde_02_time;
}

void gen_01_perform(
    const Sample * in1,
    const Sample * in2,
    number amount,
    number input_gain,
    number reverb_time,
    number diffusion,
    number lp,
    number chorus,
    number flutter_speed,
    number degradation_amount,
    number degradation_speed,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    RNBO_UNUSED(degradation_speed);
    RNBO_UNUSED(degradation_amount);
    RNBO_UNUSED(flutter_speed);
    RNBO_UNUSED(chorus);
    RNBO_UNUSED(diffusion);
    RNBO_UNUSED(input_gain);
    auto __gen_01_lp_decay_2_value = this->gen_01_lp_decay_2_value;
    auto __gen_01_lp_decay_1_value = this->gen_01_lp_decay_1_value;
    auto __gen_01_noise_state_4_value = this->gen_01_noise_state_4_value;
    auto __gen_01_noise_state_3_value = this->gen_01_noise_state_3_value;
    auto __gen_01_lfo2_phase_value = this->gen_01_lfo2_phase_value;
    auto __gen_01_lfo1_phase_value = this->gen_01_lfo1_phase_value;
    number lfo1_inc_1 = (48000 == 0. ? 0. : (number)0.5 / 48000);
    number lfo2_inc_2 = (48000 == 0. ? 0. : (number)0.3 / 48000);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number input_0 = (in1[(Index)i] + in2[(Index)i]) * 0.5;
        auto lfo1_phase_new_3 = this->wrap(__gen_01_lfo1_phase_value + lfo1_inc_1, 0, 1);
        auto lfo2_phase_new_4 = this->wrap(__gen_01_lfo2_phase_value + lfo2_inc_2, 0, 1);
        number lfo2_5 = rnbo_sin(lfo2_phase_new_4 * 6.28318530717958647692);
        number noise_state_3_new_7 = __gen_01_noise_state_3_value * (1 - 0.01) + this->gen_01_noise_6_next() * 0.01;
        number noise_state_4_new_9 = __gen_01_noise_state_4_value * (1 - 0.01) + this->gen_01_noise_8_next() * 0.01;
        number degradation_5_10 = 1 - rnbo_abs(noise_state_3_new_7) * 0.02;
        number degradation_6_11 = 1 - rnbo_abs(noise_state_4_new_9) * 0.02;
        number ap1_out_12 = this->gen_01_ap1_read(113, 0);
        number ap1_in_13 = input_0 + ap1_out_12 * 0.625;
        number ap1_write_14 = ap1_in_13 * -0.625 + ap1_out_12;
        number ap2_out_15 = this->gen_01_ap2_read(162, 0);
        number ap2_in_16 = ap1_write_14 + ap2_out_15 * 0.625;
        number ap2_write_17 = ap2_in_16 * -0.625 + ap2_out_15;
        number ap3_out_18 = this->gen_01_ap3_read(241, 0);
        number ap3_in_19 = ap2_write_17 + ap3_out_18 * 0.625;
        number ap3_write_20 = ap3_in_19 * -0.625 + ap3_out_18;
        number ap4_out_21 = this->gen_01_ap4_read(399, 0);
        number ap4_in_22 = ap3_write_20 + ap4_out_21 * 0.625;
        number apout_23 = ap4_in_22 * -0.625 + ap4_out_21;
        number del2_mod_24 = this->gen_01_del2_read(4680 + lfo2_5 * 100 * 1, 0) * reverb_time;
        number lp1_out_25 = del2_mod_24 + (__gen_01_lp_decay_1_value - del2_mod_24) * lp;
        number dap1a_out_26 = this->gen_01_dap1a_read(1653, 0);
        number dap1a_in_27 = lp1_out_25 + dap1a_out_26 * -0.625;
        number dap1a_write_28 = dap1a_in_27 * 0.625 + dap1a_out_26;
        number dap1b_out_29 = this->gen_01_dap1b_read(2038, 0);
        number dap1b_in_30 = dap1a_write_28 + dap1b_out_29 * 0.625;
        number dap1b_write_31 = dap1b_in_30 * -0.625 + dap1b_out_29;
        number del1_read_32 = this->gen_01_del1_read(3411, 0) * reverb_time;
        number lp2_out_33 = del1_read_32 + (__gen_01_lp_decay_2_value - del1_read_32) * lp;
        number dap2a_out_34 = this->gen_01_dap2a_read(1913, 0);
        number dap2a_in_35 = lp2_out_33 + dap2a_out_34 * 0.625;
        number dap2a_write_36 = dap2a_in_35 * -0.625 + dap2a_out_34;
        number dap2b_out_37 = this->gen_01_dap2b_read(1663, 0);
        number dap2b_in_38 = dap2a_write_36 + dap2b_out_37 * -0.625;
        number dap2b_write_39 = dap2b_in_38 * 0.625 + dap2b_out_37;
        number expr_7_40 = rnbo_tanh(in1[(Index)i] + (dap1b_write_31 - in1[(Index)i]) * amount);
        number expr_8_41 = rnbo_tanh(in2[(Index)i] + (dap2b_write_39 - in2[(Index)i]) * amount);
        this->gen_01_del1_write((apout_23 + rnbo_tanh(dap1b_write_31)) * degradation_5_10);
        this->gen_01_del2_write((apout_23 + rnbo_tanh(dap2b_write_39)) * degradation_6_11);
        this->gen_01_dap1a_write(dap1a_in_27);
        this->gen_01_dap1b_write(dap1b_in_30);
        this->gen_01_dap2a_write(dap2a_in_35);
        this->gen_01_dap2b_write(dap2b_in_38);
        this->gen_01_ap1_write(ap1_in_13);
        this->gen_01_ap2_write(ap2_in_16);
        this->gen_01_ap3_write(ap3_in_19);
        this->gen_01_ap4_write(ap4_in_22);
        __gen_01_lp_decay_1_value = lp1_out_25;
        __gen_01_lp_decay_2_value = lp2_out_33;
        __gen_01_lfo1_phase_value = lfo1_phase_new_3;
        __gen_01_lfo2_phase_value = lfo2_phase_new_4;
        __gen_01_noise_state_3_value = noise_state_3_new_7;
        __gen_01_noise_state_4_value = noise_state_4_new_9;
        number dcblock_9_43 = this->gen_01_dcblock_42_next(expr_7_40, 0.9997);
        out1[(Index)i] = dcblock_9_43;
        number dcblock_10_45 = this->gen_01_dcblock_44_next(expr_8_41, 0.9997);
        out2[(Index)i] = dcblock_10_45;
        this->gen_01_ap1_step();
        this->gen_01_ap2_step();
        this->gen_01_ap3_step();
        this->gen_01_ap4_step();
        this->gen_01_dap1a_step();
        this->gen_01_dap1b_step();
        this->gen_01_del1_step();
        this->gen_01_dap2a_step();
        this->gen_01_dap2b_step();
        this->gen_01_del2_step();
    }

    this->gen_01_lfo1_phase_value = __gen_01_lfo1_phase_value;
    this->gen_01_lfo2_phase_value = __gen_01_lfo2_phase_value;
    this->gen_01_noise_state_3_value = __gen_01_noise_state_3_value;
    this->gen_01_noise_state_4_value = __gen_01_noise_state_4_value;
    this->gen_01_lp_decay_1_value = __gen_01_lp_decay_1_value;
    this->gen_01_lp_decay_2_value = __gen_01_lp_decay_2_value;
}

void dspexpr_01_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_02_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
}

void toggle_01_value_number_setter(number v) {
    this->toggle_01_value_number = (v != 0 ? 1 : 0);
}

void toggle_02_value_number_setter(number v) {
    this->toggle_02_value_number = (v != 0 ? 1 : 0);
}

void toggle_01_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->toggle_01_value_number;
}

void toggle_01_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->toggle_01_value_number_set(preset["value"]);
}

void param_01_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_01_value;
}

void param_01_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_01_value_set(preset["value"]);
}

void param_02_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_02_value;
}

void param_02_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_02_value_set(preset["value"]);
}

void toggle_02_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->toggle_02_value_number;
}

void toggle_02_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->toggle_02_value_number_set(preset["value"]);
}

void param_03_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_03_value;
}

void param_03_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_03_value_set(preset["value"]);
}

void param_04_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_04_value;
}

void param_04_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_04_value_set(preset["value"]);
}

void param_05_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_05_value;
}

void param_05_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_05_value_set(preset["value"]);
}

void gen_01_ap1_step() {
    this->gen_01_ap1_reader++;

    if (this->gen_01_ap1_reader >= (Int)(this->gen_01_ap1_buffer->getSize()))
        this->gen_01_ap1_reader = 0;
}

number gen_01_ap1_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_ap1_buffer->getSize()) + this->gen_01_ap1_reader - ((113 > this->gen_01_ap1__maxdelay ? this->gen_01_ap1__maxdelay : (113 < (this->gen_01_ap1_reader != this->gen_01_ap1_writer) ? this->gen_01_ap1_reader != this->gen_01_ap1_writer : 113)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(
            frac,
            this->gen_01_ap1_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap1_wrap))),
            this->gen_01_ap1_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_ap1_wrap)))
        );
    }

    number r = (Int)(this->gen_01_ap1_buffer->getSize()) + this->gen_01_ap1_reader - ((113 > this->gen_01_ap1__maxdelay ? this->gen_01_ap1__maxdelay : (113 < (this->gen_01_ap1_reader != this->gen_01_ap1_writer) ? this->gen_01_ap1_reader != this->gen_01_ap1_writer : 113)));
    Int index1 = (Int)(rnbo_floor(r));
    return this->gen_01_ap1_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap1_wrap)));
}

void gen_01_ap1_write(number v) {
    this->gen_01_ap1_writer = this->gen_01_ap1_reader;
    this->gen_01_ap1_buffer[(Index)this->gen_01_ap1_writer] = v;
}

number gen_01_ap1_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_ap1__maxdelay : size);
    number val = this->gen_01_ap1_read(effectiveSize, 0);
    this->gen_01_ap1_write(v);
    this->gen_01_ap1_step();
    return val;
}

array<Index, 2> gen_01_ap1_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_ap1_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_ap1_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_ap1_init() {
    auto result = this->gen_01_ap1_calcSizeInSamples();
    this->gen_01_ap1__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap1_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_ap1_wrap = requestedSizeInSamples - 1;
}

void gen_01_ap1_clear() {
    this->gen_01_ap1_buffer->setZero();
}

void gen_01_ap1_reset() {
    auto result = this->gen_01_ap1_calcSizeInSamples();
    this->gen_01_ap1__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap1_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_ap1_buffer);
    this->gen_01_ap1_wrap = this->gen_01_ap1_buffer->getSize() - 1;
    this->gen_01_ap1_clear();

    if (this->gen_01_ap1_reader >= this->gen_01_ap1__maxdelay || this->gen_01_ap1_writer >= this->gen_01_ap1__maxdelay) {
        this->gen_01_ap1_reader = 0;
        this->gen_01_ap1_writer = 0;
    }
}

void gen_01_ap1_dspsetup() {
    this->gen_01_ap1_reset();
}

number gen_01_ap1_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 114;
}

number gen_01_ap1_size() {
    return this->gen_01_ap1__maxdelay;
}

void gen_01_ap2_step() {
    this->gen_01_ap2_reader++;

    if (this->gen_01_ap2_reader >= (Int)(this->gen_01_ap2_buffer->getSize()))
        this->gen_01_ap2_reader = 0;
}

number gen_01_ap2_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_ap2_buffer->getSize()) + this->gen_01_ap2_reader - ((162 > this->gen_01_ap2__maxdelay ? this->gen_01_ap2__maxdelay : (162 < (this->gen_01_ap2_reader != this->gen_01_ap2_writer) ? this->gen_01_ap2_reader != this->gen_01_ap2_writer : 162)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(
            frac,
            this->gen_01_ap2_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap2_wrap))),
            this->gen_01_ap2_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_ap2_wrap)))
        );
    }

    number r = (Int)(this->gen_01_ap2_buffer->getSize()) + this->gen_01_ap2_reader - ((162 > this->gen_01_ap2__maxdelay ? this->gen_01_ap2__maxdelay : (162 < (this->gen_01_ap2_reader != this->gen_01_ap2_writer) ? this->gen_01_ap2_reader != this->gen_01_ap2_writer : 162)));
    Int index1 = (Int)(rnbo_floor(r));
    return this->gen_01_ap2_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap2_wrap)));
}

void gen_01_ap2_write(number v) {
    this->gen_01_ap2_writer = this->gen_01_ap2_reader;
    this->gen_01_ap2_buffer[(Index)this->gen_01_ap2_writer] = v;
}

number gen_01_ap2_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_ap2__maxdelay : size);
    number val = this->gen_01_ap2_read(effectiveSize, 0);
    this->gen_01_ap2_write(v);
    this->gen_01_ap2_step();
    return val;
}

array<Index, 2> gen_01_ap2_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_ap2_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_ap2_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_ap2_init() {
    auto result = this->gen_01_ap2_calcSizeInSamples();
    this->gen_01_ap2__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap2_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_ap2_wrap = requestedSizeInSamples - 1;
}

void gen_01_ap2_clear() {
    this->gen_01_ap2_buffer->setZero();
}

void gen_01_ap2_reset() {
    auto result = this->gen_01_ap2_calcSizeInSamples();
    this->gen_01_ap2__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap2_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_ap2_buffer);
    this->gen_01_ap2_wrap = this->gen_01_ap2_buffer->getSize() - 1;
    this->gen_01_ap2_clear();

    if (this->gen_01_ap2_reader >= this->gen_01_ap2__maxdelay || this->gen_01_ap2_writer >= this->gen_01_ap2__maxdelay) {
        this->gen_01_ap2_reader = 0;
        this->gen_01_ap2_writer = 0;
    }
}

void gen_01_ap2_dspsetup() {
    this->gen_01_ap2_reset();
}

number gen_01_ap2_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 163;
}

number gen_01_ap2_size() {
    return this->gen_01_ap2__maxdelay;
}

void gen_01_ap3_step() {
    this->gen_01_ap3_reader++;

    if (this->gen_01_ap3_reader >= (Int)(this->gen_01_ap3_buffer->getSize()))
        this->gen_01_ap3_reader = 0;
}

number gen_01_ap3_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_ap3_buffer->getSize()) + this->gen_01_ap3_reader - ((241 > this->gen_01_ap3__maxdelay ? this->gen_01_ap3__maxdelay : (241 < (this->gen_01_ap3_reader != this->gen_01_ap3_writer) ? this->gen_01_ap3_reader != this->gen_01_ap3_writer : 241)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(
            frac,
            this->gen_01_ap3_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap3_wrap))),
            this->gen_01_ap3_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_ap3_wrap)))
        );
    }

    number r = (Int)(this->gen_01_ap3_buffer->getSize()) + this->gen_01_ap3_reader - ((241 > this->gen_01_ap3__maxdelay ? this->gen_01_ap3__maxdelay : (241 < (this->gen_01_ap3_reader != this->gen_01_ap3_writer) ? this->gen_01_ap3_reader != this->gen_01_ap3_writer : 241)));
    Int index1 = (Int)(rnbo_floor(r));
    return this->gen_01_ap3_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap3_wrap)));
}

void gen_01_ap3_write(number v) {
    this->gen_01_ap3_writer = this->gen_01_ap3_reader;
    this->gen_01_ap3_buffer[(Index)this->gen_01_ap3_writer] = v;
}

number gen_01_ap3_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_ap3__maxdelay : size);
    number val = this->gen_01_ap3_read(effectiveSize, 0);
    this->gen_01_ap3_write(v);
    this->gen_01_ap3_step();
    return val;
}

array<Index, 2> gen_01_ap3_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_ap3_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_ap3_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_ap3_init() {
    auto result = this->gen_01_ap3_calcSizeInSamples();
    this->gen_01_ap3__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap3_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_ap3_wrap = requestedSizeInSamples - 1;
}

void gen_01_ap3_clear() {
    this->gen_01_ap3_buffer->setZero();
}

void gen_01_ap3_reset() {
    auto result = this->gen_01_ap3_calcSizeInSamples();
    this->gen_01_ap3__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap3_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_ap3_buffer);
    this->gen_01_ap3_wrap = this->gen_01_ap3_buffer->getSize() - 1;
    this->gen_01_ap3_clear();

    if (this->gen_01_ap3_reader >= this->gen_01_ap3__maxdelay || this->gen_01_ap3_writer >= this->gen_01_ap3__maxdelay) {
        this->gen_01_ap3_reader = 0;
        this->gen_01_ap3_writer = 0;
    }
}

void gen_01_ap3_dspsetup() {
    this->gen_01_ap3_reset();
}

number gen_01_ap3_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 242;
}

number gen_01_ap3_size() {
    return this->gen_01_ap3__maxdelay;
}

void gen_01_ap4_step() {
    this->gen_01_ap4_reader++;

    if (this->gen_01_ap4_reader >= (Int)(this->gen_01_ap4_buffer->getSize()))
        this->gen_01_ap4_reader = 0;
}

number gen_01_ap4_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_ap4_buffer->getSize()) + this->gen_01_ap4_reader - ((399 > this->gen_01_ap4__maxdelay ? this->gen_01_ap4__maxdelay : (399 < (this->gen_01_ap4_reader != this->gen_01_ap4_writer) ? this->gen_01_ap4_reader != this->gen_01_ap4_writer : 399)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(
            frac,
            this->gen_01_ap4_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap4_wrap))),
            this->gen_01_ap4_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_ap4_wrap)))
        );
    }

    number r = (Int)(this->gen_01_ap4_buffer->getSize()) + this->gen_01_ap4_reader - ((399 > this->gen_01_ap4__maxdelay ? this->gen_01_ap4__maxdelay : (399 < (this->gen_01_ap4_reader != this->gen_01_ap4_writer) ? this->gen_01_ap4_reader != this->gen_01_ap4_writer : 399)));
    Int index1 = (Int)(rnbo_floor(r));
    return this->gen_01_ap4_buffer->getSample(0, (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_ap4_wrap)));
}

void gen_01_ap4_write(number v) {
    this->gen_01_ap4_writer = this->gen_01_ap4_reader;
    this->gen_01_ap4_buffer[(Index)this->gen_01_ap4_writer] = v;
}

number gen_01_ap4_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_ap4__maxdelay : size);
    number val = this->gen_01_ap4_read(effectiveSize, 0);
    this->gen_01_ap4_write(v);
    this->gen_01_ap4_step();
    return val;
}

array<Index, 2> gen_01_ap4_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_ap4_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_ap4_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_ap4_init() {
    auto result = this->gen_01_ap4_calcSizeInSamples();
    this->gen_01_ap4__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap4_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_ap4_wrap = requestedSizeInSamples - 1;
}

void gen_01_ap4_clear() {
    this->gen_01_ap4_buffer->setZero();
}

void gen_01_ap4_reset() {
    auto result = this->gen_01_ap4_calcSizeInSamples();
    this->gen_01_ap4__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_ap4_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_ap4_buffer);
    this->gen_01_ap4_wrap = this->gen_01_ap4_buffer->getSize() - 1;
    this->gen_01_ap4_clear();

    if (this->gen_01_ap4_reader >= this->gen_01_ap4__maxdelay || this->gen_01_ap4_writer >= this->gen_01_ap4__maxdelay) {
        this->gen_01_ap4_reader = 0;
        this->gen_01_ap4_writer = 0;
    }
}

void gen_01_ap4_dspsetup() {
    this->gen_01_ap4_reset();
}

number gen_01_ap4_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 400;
}

number gen_01_ap4_size() {
    return this->gen_01_ap4__maxdelay;
}

void gen_01_dap1a_step() {
    this->gen_01_dap1a_reader++;

    if (this->gen_01_dap1a_reader >= (Int)(this->gen_01_dap1a_buffer->getSize()))
        this->gen_01_dap1a_reader = 0;
}

number gen_01_dap1a_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_dap1a_buffer->getSize()) + this->gen_01_dap1a_reader - ((1653 > this->gen_01_dap1a__maxdelay ? this->gen_01_dap1a__maxdelay : (1653 < (this->gen_01_dap1a_reader != this->gen_01_dap1a_writer) ? this->gen_01_dap1a_reader != this->gen_01_dap1a_writer : 1653)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_dap1a_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap1a_wrap))
        ), this->gen_01_dap1a_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_dap1a_wrap))
        ));
    }

    number r = (Int)(this->gen_01_dap1a_buffer->getSize()) + this->gen_01_dap1a_reader - ((1653 > this->gen_01_dap1a__maxdelay ? this->gen_01_dap1a__maxdelay : (1653 < (this->gen_01_dap1a_reader != this->gen_01_dap1a_writer) ? this->gen_01_dap1a_reader != this->gen_01_dap1a_writer : 1653)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_dap1a_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap1a_wrap))
    );
}

void gen_01_dap1a_write(number v) {
    this->gen_01_dap1a_writer = this->gen_01_dap1a_reader;
    this->gen_01_dap1a_buffer[(Index)this->gen_01_dap1a_writer] = v;
}

number gen_01_dap1a_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_dap1a__maxdelay : size);
    number val = this->gen_01_dap1a_read(effectiveSize, 0);
    this->gen_01_dap1a_write(v);
    this->gen_01_dap1a_step();
    return val;
}

array<Index, 2> gen_01_dap1a_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_dap1a_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_dap1a_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_dap1a_init() {
    auto result = this->gen_01_dap1a_calcSizeInSamples();
    this->gen_01_dap1a__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap1a_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_dap1a_wrap = requestedSizeInSamples - 1;
}

void gen_01_dap1a_clear() {
    this->gen_01_dap1a_buffer->setZero();
}

void gen_01_dap1a_reset() {
    auto result = this->gen_01_dap1a_calcSizeInSamples();
    this->gen_01_dap1a__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap1a_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_dap1a_buffer);
    this->gen_01_dap1a_wrap = this->gen_01_dap1a_buffer->getSize() - 1;
    this->gen_01_dap1a_clear();

    if (this->gen_01_dap1a_reader >= this->gen_01_dap1a__maxdelay || this->gen_01_dap1a_writer >= this->gen_01_dap1a__maxdelay) {
        this->gen_01_dap1a_reader = 0;
        this->gen_01_dap1a_writer = 0;
    }
}

void gen_01_dap1a_dspsetup() {
    this->gen_01_dap1a_reset();
}

number gen_01_dap1a_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 1654;
}

number gen_01_dap1a_size() {
    return this->gen_01_dap1a__maxdelay;
}

void gen_01_dap1b_step() {
    this->gen_01_dap1b_reader++;

    if (this->gen_01_dap1b_reader >= (Int)(this->gen_01_dap1b_buffer->getSize()))
        this->gen_01_dap1b_reader = 0;
}

number gen_01_dap1b_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_dap1b_buffer->getSize()) + this->gen_01_dap1b_reader - ((2038 > this->gen_01_dap1b__maxdelay ? this->gen_01_dap1b__maxdelay : (2038 < (this->gen_01_dap1b_reader != this->gen_01_dap1b_writer) ? this->gen_01_dap1b_reader != this->gen_01_dap1b_writer : 2038)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_dap1b_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap1b_wrap))
        ), this->gen_01_dap1b_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_dap1b_wrap))
        ));
    }

    number r = (Int)(this->gen_01_dap1b_buffer->getSize()) + this->gen_01_dap1b_reader - ((2038 > this->gen_01_dap1b__maxdelay ? this->gen_01_dap1b__maxdelay : (2038 < (this->gen_01_dap1b_reader != this->gen_01_dap1b_writer) ? this->gen_01_dap1b_reader != this->gen_01_dap1b_writer : 2038)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_dap1b_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap1b_wrap))
    );
}

void gen_01_dap1b_write(number v) {
    this->gen_01_dap1b_writer = this->gen_01_dap1b_reader;
    this->gen_01_dap1b_buffer[(Index)this->gen_01_dap1b_writer] = v;
}

number gen_01_dap1b_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_dap1b__maxdelay : size);
    number val = this->gen_01_dap1b_read(effectiveSize, 0);
    this->gen_01_dap1b_write(v);
    this->gen_01_dap1b_step();
    return val;
}

array<Index, 2> gen_01_dap1b_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_dap1b_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_dap1b_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_dap1b_init() {
    auto result = this->gen_01_dap1b_calcSizeInSamples();
    this->gen_01_dap1b__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap1b_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_dap1b_wrap = requestedSizeInSamples - 1;
}

void gen_01_dap1b_clear() {
    this->gen_01_dap1b_buffer->setZero();
}

void gen_01_dap1b_reset() {
    auto result = this->gen_01_dap1b_calcSizeInSamples();
    this->gen_01_dap1b__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap1b_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_dap1b_buffer);
    this->gen_01_dap1b_wrap = this->gen_01_dap1b_buffer->getSize() - 1;
    this->gen_01_dap1b_clear();

    if (this->gen_01_dap1b_reader >= this->gen_01_dap1b__maxdelay || this->gen_01_dap1b_writer >= this->gen_01_dap1b__maxdelay) {
        this->gen_01_dap1b_reader = 0;
        this->gen_01_dap1b_writer = 0;
    }
}

void gen_01_dap1b_dspsetup() {
    this->gen_01_dap1b_reset();
}

number gen_01_dap1b_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 2039;
}

number gen_01_dap1b_size() {
    return this->gen_01_dap1b__maxdelay;
}

void gen_01_del1_step() {
    this->gen_01_del1_reader++;

    if (this->gen_01_del1_reader >= (Int)(this->gen_01_del1_buffer->getSize()))
        this->gen_01_del1_reader = 0;
}

number gen_01_del1_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_del1_buffer->getSize()) + this->gen_01_del1_reader - ((3411 > this->gen_01_del1__maxdelay ? this->gen_01_del1__maxdelay : (3411 < (this->gen_01_del1_reader != this->gen_01_del1_writer) ? this->gen_01_del1_reader != this->gen_01_del1_writer : 3411)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_del1_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_del1_wrap))
        ), this->gen_01_del1_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_del1_wrap))
        ));
    }

    number r = (Int)(this->gen_01_del1_buffer->getSize()) + this->gen_01_del1_reader - ((3411 > this->gen_01_del1__maxdelay ? this->gen_01_del1__maxdelay : (3411 < (this->gen_01_del1_reader != this->gen_01_del1_writer) ? this->gen_01_del1_reader != this->gen_01_del1_writer : 3411)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_del1_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_del1_wrap))
    );
}

void gen_01_del1_write(number v) {
    this->gen_01_del1_writer = this->gen_01_del1_reader;
    this->gen_01_del1_buffer[(Index)this->gen_01_del1_writer] = v;
}

number gen_01_del1_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_del1__maxdelay : size);
    number val = this->gen_01_del1_read(effectiveSize, 0);
    this->gen_01_del1_write(v);
    this->gen_01_del1_step();
    return val;
}

array<Index, 2> gen_01_del1_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_del1_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_del1_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_del1_init() {
    auto result = this->gen_01_del1_calcSizeInSamples();
    this->gen_01_del1__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_del1_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_del1_wrap = requestedSizeInSamples - 1;
}

void gen_01_del1_clear() {
    this->gen_01_del1_buffer->setZero();
}

void gen_01_del1_reset() {
    auto result = this->gen_01_del1_calcSizeInSamples();
    this->gen_01_del1__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_del1_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_del1_buffer);
    this->gen_01_del1_wrap = this->gen_01_del1_buffer->getSize() - 1;
    this->gen_01_del1_clear();

    if (this->gen_01_del1_reader >= this->gen_01_del1__maxdelay || this->gen_01_del1_writer >= this->gen_01_del1__maxdelay) {
        this->gen_01_del1_reader = 0;
        this->gen_01_del1_writer = 0;
    }
}

void gen_01_del1_dspsetup() {
    this->gen_01_del1_reset();
}

number gen_01_del1_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 3412;
}

number gen_01_del1_size() {
    return this->gen_01_del1__maxdelay;
}

void gen_01_dap2a_step() {
    this->gen_01_dap2a_reader++;

    if (this->gen_01_dap2a_reader >= (Int)(this->gen_01_dap2a_buffer->getSize()))
        this->gen_01_dap2a_reader = 0;
}

number gen_01_dap2a_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_dap2a_buffer->getSize()) + this->gen_01_dap2a_reader - ((1913 > this->gen_01_dap2a__maxdelay ? this->gen_01_dap2a__maxdelay : (1913 < (this->gen_01_dap2a_reader != this->gen_01_dap2a_writer) ? this->gen_01_dap2a_reader != this->gen_01_dap2a_writer : 1913)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_dap2a_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap2a_wrap))
        ), this->gen_01_dap2a_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_dap2a_wrap))
        ));
    }

    number r = (Int)(this->gen_01_dap2a_buffer->getSize()) + this->gen_01_dap2a_reader - ((1913 > this->gen_01_dap2a__maxdelay ? this->gen_01_dap2a__maxdelay : (1913 < (this->gen_01_dap2a_reader != this->gen_01_dap2a_writer) ? this->gen_01_dap2a_reader != this->gen_01_dap2a_writer : 1913)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_dap2a_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap2a_wrap))
    );
}

void gen_01_dap2a_write(number v) {
    this->gen_01_dap2a_writer = this->gen_01_dap2a_reader;
    this->gen_01_dap2a_buffer[(Index)this->gen_01_dap2a_writer] = v;
}

number gen_01_dap2a_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_dap2a__maxdelay : size);
    number val = this->gen_01_dap2a_read(effectiveSize, 0);
    this->gen_01_dap2a_write(v);
    this->gen_01_dap2a_step();
    return val;
}

array<Index, 2> gen_01_dap2a_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_dap2a_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_dap2a_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_dap2a_init() {
    auto result = this->gen_01_dap2a_calcSizeInSamples();
    this->gen_01_dap2a__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap2a_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_dap2a_wrap = requestedSizeInSamples - 1;
}

void gen_01_dap2a_clear() {
    this->gen_01_dap2a_buffer->setZero();
}

void gen_01_dap2a_reset() {
    auto result = this->gen_01_dap2a_calcSizeInSamples();
    this->gen_01_dap2a__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap2a_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_dap2a_buffer);
    this->gen_01_dap2a_wrap = this->gen_01_dap2a_buffer->getSize() - 1;
    this->gen_01_dap2a_clear();

    if (this->gen_01_dap2a_reader >= this->gen_01_dap2a__maxdelay || this->gen_01_dap2a_writer >= this->gen_01_dap2a__maxdelay) {
        this->gen_01_dap2a_reader = 0;
        this->gen_01_dap2a_writer = 0;
    }
}

void gen_01_dap2a_dspsetup() {
    this->gen_01_dap2a_reset();
}

number gen_01_dap2a_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 1914;
}

number gen_01_dap2a_size() {
    return this->gen_01_dap2a__maxdelay;
}

void gen_01_dap2b_step() {
    this->gen_01_dap2b_reader++;

    if (this->gen_01_dap2b_reader >= (Int)(this->gen_01_dap2b_buffer->getSize()))
        this->gen_01_dap2b_reader = 0;
}

number gen_01_dap2b_read(number size, Int interp) {
    RNBO_UNUSED(interp);
    RNBO_UNUSED(size);

    {
        number r = (Int)(this->gen_01_dap2b_buffer->getSize()) + this->gen_01_dap2b_reader - ((1663 > this->gen_01_dap2b__maxdelay ? this->gen_01_dap2b__maxdelay : (1663 < (this->gen_01_dap2b_reader != this->gen_01_dap2b_writer) ? this->gen_01_dap2b_reader != this->gen_01_dap2b_writer : 1663)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_dap2b_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap2b_wrap))
        ), this->gen_01_dap2b_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_dap2b_wrap))
        ));
    }

    number r = (Int)(this->gen_01_dap2b_buffer->getSize()) + this->gen_01_dap2b_reader - ((1663 > this->gen_01_dap2b__maxdelay ? this->gen_01_dap2b__maxdelay : (1663 < (this->gen_01_dap2b_reader != this->gen_01_dap2b_writer) ? this->gen_01_dap2b_reader != this->gen_01_dap2b_writer : 1663)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_dap2b_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_dap2b_wrap))
    );
}

void gen_01_dap2b_write(number v) {
    this->gen_01_dap2b_writer = this->gen_01_dap2b_reader;
    this->gen_01_dap2b_buffer[(Index)this->gen_01_dap2b_writer] = v;
}

number gen_01_dap2b_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_dap2b__maxdelay : size);
    number val = this->gen_01_dap2b_read(effectiveSize, 0);
    this->gen_01_dap2b_write(v);
    this->gen_01_dap2b_step();
    return val;
}

array<Index, 2> gen_01_dap2b_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_dap2b_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_dap2b_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_dap2b_init() {
    auto result = this->gen_01_dap2b_calcSizeInSamples();
    this->gen_01_dap2b__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap2b_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_dap2b_wrap = requestedSizeInSamples - 1;
}

void gen_01_dap2b_clear() {
    this->gen_01_dap2b_buffer->setZero();
}

void gen_01_dap2b_reset() {
    auto result = this->gen_01_dap2b_calcSizeInSamples();
    this->gen_01_dap2b__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_dap2b_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_dap2b_buffer);
    this->gen_01_dap2b_wrap = this->gen_01_dap2b_buffer->getSize() - 1;
    this->gen_01_dap2b_clear();

    if (this->gen_01_dap2b_reader >= this->gen_01_dap2b__maxdelay || this->gen_01_dap2b_writer >= this->gen_01_dap2b__maxdelay) {
        this->gen_01_dap2b_reader = 0;
        this->gen_01_dap2b_writer = 0;
    }
}

void gen_01_dap2b_dspsetup() {
    this->gen_01_dap2b_reset();
}

number gen_01_dap2b_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 1664;
}

number gen_01_dap2b_size() {
    return this->gen_01_dap2b__maxdelay;
}

void gen_01_del2_step() {
    this->gen_01_del2_reader++;

    if (this->gen_01_del2_reader >= (Int)(this->gen_01_del2_buffer->getSize()))
        this->gen_01_del2_reader = 0;
}

number gen_01_del2_read(number size, Int interp) {
    RNBO_UNUSED(interp);

    {
        number r = (Int)(this->gen_01_del2_buffer->getSize()) + this->gen_01_del2_reader - ((size > this->gen_01_del2__maxdelay ? this->gen_01_del2__maxdelay : (size < (this->gen_01_del2_reader != this->gen_01_del2_writer) ? this->gen_01_del2_reader != this->gen_01_del2_writer : size)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_del2_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_del2_wrap))
        ), this->gen_01_del2_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_del2_wrap))
        ));
    }

    number r = (Int)(this->gen_01_del2_buffer->getSize()) + this->gen_01_del2_reader - ((size > this->gen_01_del2__maxdelay ? this->gen_01_del2__maxdelay : (size < (this->gen_01_del2_reader != this->gen_01_del2_writer) ? this->gen_01_del2_reader != this->gen_01_del2_writer : size)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_del2_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_del2_wrap))
    );
}

void gen_01_del2_write(number v) {
    this->gen_01_del2_writer = this->gen_01_del2_reader;
    this->gen_01_del2_buffer[(Index)this->gen_01_del2_writer] = v;
}

number gen_01_del2_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_del2__maxdelay : size);
    number val = this->gen_01_del2_read(effectiveSize, 0);
    this->gen_01_del2_write(v);
    this->gen_01_del2_step();
    return val;
}

array<Index, 2> gen_01_del2_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_del2_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_del2_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_del2_init() {
    auto result = this->gen_01_del2_calcSizeInSamples();
    this->gen_01_del2__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_del2_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_del2_wrap = requestedSizeInSamples - 1;
}

void gen_01_del2_clear() {
    this->gen_01_del2_buffer->setZero();
}

void gen_01_del2_reset() {
    auto result = this->gen_01_del2_calcSizeInSamples();
    this->gen_01_del2__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_del2_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_del2_buffer);
    this->gen_01_del2_wrap = this->gen_01_del2_buffer->getSize() - 1;
    this->gen_01_del2_clear();

    if (this->gen_01_del2_reader >= this->gen_01_del2__maxdelay || this->gen_01_del2_writer >= this->gen_01_del2__maxdelay) {
        this->gen_01_del2_reader = 0;
        this->gen_01_del2_writer = 0;
    }
}

void gen_01_del2_dspsetup() {
    this->gen_01_del2_reset();
}

number gen_01_del2_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 4783;
}

number gen_01_del2_size() {
    return this->gen_01_del2__maxdelay;
}

number gen_01_lp_decay_1_getvalue() {
    return this->gen_01_lp_decay_1_value;
}

void gen_01_lp_decay_1_setvalue(number val) {
    this->gen_01_lp_decay_1_value = val;
}

void gen_01_lp_decay_1_reset() {
    this->gen_01_lp_decay_1_value = 0;
}

void gen_01_lp_decay_1_init() {
    this->gen_01_lp_decay_1_value = 0;
}

number gen_01_lp_decay_2_getvalue() {
    return this->gen_01_lp_decay_2_value;
}

void gen_01_lp_decay_2_setvalue(number val) {
    this->gen_01_lp_decay_2_value = val;
}

void gen_01_lp_decay_2_reset() {
    this->gen_01_lp_decay_2_value = 0;
}

void gen_01_lp_decay_2_init() {
    this->gen_01_lp_decay_2_value = 0;
}

number gen_01_lfo1_phase_getvalue() {
    return this->gen_01_lfo1_phase_value;
}

void gen_01_lfo1_phase_setvalue(number val) {
    this->gen_01_lfo1_phase_value = val;
}

void gen_01_lfo1_phase_reset() {
    this->gen_01_lfo1_phase_value = 0;
}

void gen_01_lfo1_phase_init() {
    this->gen_01_lfo1_phase_value = 0;
}

number gen_01_lfo2_phase_getvalue() {
    return this->gen_01_lfo2_phase_value;
}

void gen_01_lfo2_phase_setvalue(number val) {
    this->gen_01_lfo2_phase_value = val;
}

void gen_01_lfo2_phase_reset() {
    this->gen_01_lfo2_phase_value = 0;
}

void gen_01_lfo2_phase_init() {
    this->gen_01_lfo2_phase_value = 0;
}

number gen_01_noise_state_3_getvalue() {
    return this->gen_01_noise_state_3_value;
}

void gen_01_noise_state_3_setvalue(number val) {
    this->gen_01_noise_state_3_value = val;
}

void gen_01_noise_state_3_reset() {
    this->gen_01_noise_state_3_value = 0;
}

void gen_01_noise_state_3_init() {
    this->gen_01_noise_state_3_value = 0;
}

number gen_01_noise_state_4_getvalue() {
    return this->gen_01_noise_state_4_value;
}

void gen_01_noise_state_4_setvalue(number val) {
    this->gen_01_noise_state_4_value = val;
}

void gen_01_noise_state_4_reset() {
    this->gen_01_noise_state_4_value = 0;
}

void gen_01_noise_state_4_init() {
    this->gen_01_noise_state_4_value = 0;
}

void gen_01_noise_6_reset() {
    xoshiro_reset(
        systemticks() + this->voice() + this->random(0, 10000),
        this->gen_01_noise_6_state
    );
}

void gen_01_noise_6_init() {
    this->gen_01_noise_6_reset();
}

void gen_01_noise_6_seed(number v) {
    xoshiro_reset(v, this->gen_01_noise_6_state);
}

number gen_01_noise_6_next() {
    return xoshiro_next(this->gen_01_noise_6_state);
}

void gen_01_noise_8_reset() {
    xoshiro_reset(
        systemticks() + this->voice() + this->random(0, 10000),
        this->gen_01_noise_8_state
    );
}

void gen_01_noise_8_init() {
    this->gen_01_noise_8_reset();
}

void gen_01_noise_8_seed(number v) {
    xoshiro_reset(v, this->gen_01_noise_8_state);
}

number gen_01_noise_8_next() {
    return xoshiro_next(this->gen_01_noise_8_state);
}

number gen_01_dcblock_42_next(number x, number gain) {
    RNBO_UNUSED(gain);
    number y = x - this->gen_01_dcblock_42_xm1 + this->gen_01_dcblock_42_ym1 * 0.9997;
    this->gen_01_dcblock_42_xm1 = x;
    this->gen_01_dcblock_42_ym1 = y;
    return y;
}

void gen_01_dcblock_42_reset() {
    this->gen_01_dcblock_42_xm1 = 0;
    this->gen_01_dcblock_42_ym1 = 0;
}

void gen_01_dcblock_42_dspsetup() {
    this->gen_01_dcblock_42_reset();
}

number gen_01_dcblock_44_next(number x, number gain) {
    RNBO_UNUSED(gain);
    number y = x - this->gen_01_dcblock_44_xm1 + this->gen_01_dcblock_44_ym1 * 0.9997;
    this->gen_01_dcblock_44_xm1 = x;
    this->gen_01_dcblock_44_ym1 = y;
    return y;
}

void gen_01_dcblock_44_reset() {
    this->gen_01_dcblock_44_xm1 = 0;
    this->gen_01_dcblock_44_ym1 = 0;
}

void gen_01_dcblock_44_dspsetup() {
    this->gen_01_dcblock_44_reset();
}

void gen_01_dspsetup(bool force) {
    if ((bool)(this->gen_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->gen_01_setupDone = true;
    this->gen_01_ap1_dspsetup();
    this->gen_01_ap2_dspsetup();
    this->gen_01_ap3_dspsetup();
    this->gen_01_ap4_dspsetup();
    this->gen_01_dap1a_dspsetup();
    this->gen_01_dap1b_dspsetup();
    this->gen_01_del1_dspsetup();
    this->gen_01_dap2a_dspsetup();
    this->gen_01_dap2b_dspsetup();
    this->gen_01_del2_dspsetup();
    this->gen_01_dcblock_42_dspsetup();
    this->gen_01_dcblock_44_dspsetup();
}

void param_06_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_06_value;
}

void param_06_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_06_value_set(preset["value"]);
}

void param_07_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_07_value;
}

void param_07_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_07_value_set(preset["value"]);
}

void globaltransport_advance() {}

void globaltransport_dspsetup(bool ) {}

bool stackprotect_check() {
    this->stackprotect_count++;

    if (this->stackprotect_count > 128) {
        console->log("STACK OVERFLOW DETECTED - stopped processing branch !");
        return true;
    }

    return false;
}

Index getPatcherSerial() const {
    return 0;
}

void sendParameter(ParameterIndex index, bool ignoreValue) {
    this->getEngine()->notifyParameterValueChanged(index, (ignoreValue ? 0 : this->getParameterValue(index)), ignoreValue);
}

void scheduleParamInit(ParameterIndex index, Index order) {
    this->paramInitIndices->push(index);
    this->paramInitOrder->push(order);
}

void processParamInitEvents() {
    this->listquicksort(
        this->paramInitOrder,
        this->paramInitIndices,
        0,
        (int)(this->paramInitOrder->length - 1),
        true
    );

    for (Index i = 0; i < this->paramInitOrder->length; i++) {
        this->getEngine()->scheduleParameterBang(this->paramInitIndices[i], 0);
    }
}

void updateTime(MillisecondTime time, EXTERNALENGINE* engine, bool inProcess = false) {
    RNBO_UNUSED(inProcess);
    RNBO_UNUSED(engine);
    this->_currentTime = time;
    auto offset = rnbo_fround(this->msToSamps(time - this->getEngine()->getCurrentTime(), this->sr));

    if (offset >= (SampleIndex)(this->vs))
        offset = (SampleIndex)(this->vs) - 1;

    if (offset < 0)
        offset = 0;

    this->sampleOffsetIntoNextAudioBuffer = (Index)(offset);
}

void assign_defaults()
{
    toggle_01_value_number = 0;
    toggle_01_value_number_setter(toggle_01_value_number);
    select_01_test1 = 1;
    delta_01_x = 0;
    linetilde_01_time = 0;
    linetilde_01_keepramp = false;
    param_01_value = 0;
    param_02_value = 0;
    toggle_02_value_number = 0;
    toggle_02_value_number_setter(toggle_02_value_number);
    select_02_test1 = 1;
    delta_02_x = 0;
    linetilde_02_time = 0;
    linetilde_02_keepramp = false;
    param_03_value = 0;
    param_04_value = 0;
    dspexpr_01_in1 = 0;
    dspexpr_01_in2 = 0;
    param_05_value = 0;
    gen_01_in1 = 0;
    gen_01_in2 = 0;
    gen_01_amount = 0.5;
    gen_01_input_gain = 0.5;
    gen_01_reverb_time = 0.7;
    gen_01_diffusion = 0.625;
    gen_01_lp = 0.7;
    gen_01_chorus = 1;
    gen_01_flutter_speed = 1;
    gen_01_degradation_amount = 0.02;
    gen_01_degradation_speed = 0.01;
    param_06_value = 0;
    param_07_value = 0;
    dspexpr_02_in1 = 0;
    dspexpr_02_in2 = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    signals[0] = nullptr;
    signals[1] = nullptr;
    signals[2] = nullptr;
    signals[3] = nullptr;
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 48000;
    invsr = 0.000020833333333333333;
    toggle_01_lastValue = 0;
    delta_01_prev = 0;
    pack_01_data = { 0, 0 };
    linetilde_01_currentValue = 0;
    param_01_lastValue = 0;
    param_02_lastValue = 0;
    toggle_02_lastValue = 0;
    delta_02_prev = 0;
    pack_02_data = { 0, 0 };
    linetilde_02_currentValue = 0;
    param_03_lastValue = 0;
    param_04_lastValue = 0;
    param_05_lastValue = 0;
    gen_01_ap1__maxdelay = 0;
    gen_01_ap1_sizemode = 0;
    gen_01_ap1_wrap = 0;
    gen_01_ap1_reader = 0;
    gen_01_ap1_writer = 0;
    gen_01_ap2__maxdelay = 0;
    gen_01_ap2_sizemode = 0;
    gen_01_ap2_wrap = 0;
    gen_01_ap2_reader = 0;
    gen_01_ap2_writer = 0;
    gen_01_ap3__maxdelay = 0;
    gen_01_ap3_sizemode = 0;
    gen_01_ap3_wrap = 0;
    gen_01_ap3_reader = 0;
    gen_01_ap3_writer = 0;
    gen_01_ap4__maxdelay = 0;
    gen_01_ap4_sizemode = 0;
    gen_01_ap4_wrap = 0;
    gen_01_ap4_reader = 0;
    gen_01_ap4_writer = 0;
    gen_01_dap1a__maxdelay = 0;
    gen_01_dap1a_sizemode = 0;
    gen_01_dap1a_wrap = 0;
    gen_01_dap1a_reader = 0;
    gen_01_dap1a_writer = 0;
    gen_01_dap1b__maxdelay = 0;
    gen_01_dap1b_sizemode = 0;
    gen_01_dap1b_wrap = 0;
    gen_01_dap1b_reader = 0;
    gen_01_dap1b_writer = 0;
    gen_01_del1__maxdelay = 0;
    gen_01_del1_sizemode = 0;
    gen_01_del1_wrap = 0;
    gen_01_del1_reader = 0;
    gen_01_del1_writer = 0;
    gen_01_dap2a__maxdelay = 0;
    gen_01_dap2a_sizemode = 0;
    gen_01_dap2a_wrap = 0;
    gen_01_dap2a_reader = 0;
    gen_01_dap2a_writer = 0;
    gen_01_dap2b__maxdelay = 0;
    gen_01_dap2b_sizemode = 0;
    gen_01_dap2b_wrap = 0;
    gen_01_dap2b_reader = 0;
    gen_01_dap2b_writer = 0;
    gen_01_del2__maxdelay = 0;
    gen_01_del2_sizemode = 0;
    gen_01_del2_wrap = 0;
    gen_01_del2_reader = 0;
    gen_01_del2_writer = 0;
    gen_01_lp_decay_1_value = 0;
    gen_01_lp_decay_2_value = 0;
    gen_01_lfo1_phase_value = 0;
    gen_01_lfo2_phase_value = 0;
    gen_01_noise_state_3_value = 0;
    gen_01_noise_state_4_value = 0;
    gen_01_dcblock_42_xm1 = 0;
    gen_01_dcblock_42_ym1 = 0;
    gen_01_dcblock_44_xm1 = 0;
    gen_01_dcblock_44_ym1 = 0;
    gen_01_setupDone = false;
    param_06_lastValue = 0;
    param_07_lastValue = 0;
    globaltransport_tempo = nullptr;
    globaltransport_state = nullptr;
    stackprotect_count = 0;
    _voiceIndex = 0;
    _noteNumber = 0;
    isMuted = 1;
}

    // data ref strings
    struct DataRefStrings {
    	static constexpr auto& name0 = "gen_01_ap1_bufferobj";
    	static constexpr auto& file0 = "";
    	static constexpr auto& tag0 = "buffer~";
    	static constexpr auto& name1 = "gen_01_ap2_bufferobj";
    	static constexpr auto& file1 = "";
    	static constexpr auto& tag1 = "buffer~";
    	static constexpr auto& name2 = "gen_01_ap3_bufferobj";
    	static constexpr auto& file2 = "";
    	static constexpr auto& tag2 = "buffer~";
    	static constexpr auto& name3 = "gen_01_ap4_bufferobj";
    	static constexpr auto& file3 = "";
    	static constexpr auto& tag3 = "buffer~";
    	static constexpr auto& name4 = "gen_01_dap1a_bufferobj";
    	static constexpr auto& file4 = "";
    	static constexpr auto& tag4 = "buffer~";
    	static constexpr auto& name5 = "gen_01_dap1b_bufferobj";
    	static constexpr auto& file5 = "";
    	static constexpr auto& tag5 = "buffer~";
    	static constexpr auto& name6 = "gen_01_del1_bufferobj";
    	static constexpr auto& file6 = "";
    	static constexpr auto& tag6 = "buffer~";
    	static constexpr auto& name7 = "gen_01_dap2a_bufferobj";
    	static constexpr auto& file7 = "";
    	static constexpr auto& tag7 = "buffer~";
    	static constexpr auto& name8 = "gen_01_dap2b_bufferobj";
    	static constexpr auto& file8 = "";
    	static constexpr auto& tag8 = "buffer~";
    	static constexpr auto& name9 = "gen_01_del2_bufferobj";
    	static constexpr auto& file9 = "";
    	static constexpr auto& tag9 = "buffer~";
    	DataRefStrings* operator->() { return this; }
    	const DataRefStrings* operator->() const { return this; }
    };

    DataRefStrings dataRefStrings;

// member variables

    number toggle_01_value_number;
    number select_01_test1;
    number delta_01_x;
    list linetilde_01_segments;
    number linetilde_01_time;
    number linetilde_01_keepramp;
    number param_01_value;
    number param_02_value;
    number toggle_02_value_number;
    number select_02_test1;
    number delta_02_x;
    list linetilde_02_segments;
    number linetilde_02_time;
    number linetilde_02_keepramp;
    number param_03_value;
    number param_04_value;
    number dspexpr_01_in1;
    number dspexpr_01_in2;
    number param_05_value;
    number gen_01_in1;
    number gen_01_in2;
    number gen_01_amount;
    number gen_01_input_gain;
    number gen_01_reverb_time;
    number gen_01_diffusion;
    number gen_01_lp;
    number gen_01_chorus;
    number gen_01_flutter_speed;
    number gen_01_degradation_amount;
    number gen_01_degradation_speed;
    number param_06_value;
    number param_07_value;
    number dspexpr_02_in1;
    number dspexpr_02_in2;
    MillisecondTime _currentTime;
    ENGINE _internalEngine;
    UInt64 audioProcessSampleCount;
    Index sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[4];
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    number toggle_01_lastValue;
    number delta_01_prev;
    list pack_01_data;
    list linetilde_01_activeRamps;
    number linetilde_01_currentValue;
    number param_01_lastValue;
    number param_02_lastValue;
    number toggle_02_lastValue;
    number delta_02_prev;
    list pack_02_data;
    list linetilde_02_activeRamps;
    number linetilde_02_currentValue;
    number param_03_lastValue;
    number param_04_lastValue;
    number param_05_lastValue;
    Float64BufferRef gen_01_ap1_buffer;
    Index gen_01_ap1__maxdelay;
    Int gen_01_ap1_sizemode;
    Index gen_01_ap1_wrap;
    Int gen_01_ap1_reader;
    Int gen_01_ap1_writer;
    Float64BufferRef gen_01_ap2_buffer;
    Index gen_01_ap2__maxdelay;
    Int gen_01_ap2_sizemode;
    Index gen_01_ap2_wrap;
    Int gen_01_ap2_reader;
    Int gen_01_ap2_writer;
    Float64BufferRef gen_01_ap3_buffer;
    Index gen_01_ap3__maxdelay;
    Int gen_01_ap3_sizemode;
    Index gen_01_ap3_wrap;
    Int gen_01_ap3_reader;
    Int gen_01_ap3_writer;
    Float64BufferRef gen_01_ap4_buffer;
    Index gen_01_ap4__maxdelay;
    Int gen_01_ap4_sizemode;
    Index gen_01_ap4_wrap;
    Int gen_01_ap4_reader;
    Int gen_01_ap4_writer;
    Float64BufferRef gen_01_dap1a_buffer;
    Index gen_01_dap1a__maxdelay;
    Int gen_01_dap1a_sizemode;
    Index gen_01_dap1a_wrap;
    Int gen_01_dap1a_reader;
    Int gen_01_dap1a_writer;
    Float64BufferRef gen_01_dap1b_buffer;
    Index gen_01_dap1b__maxdelay;
    Int gen_01_dap1b_sizemode;
    Index gen_01_dap1b_wrap;
    Int gen_01_dap1b_reader;
    Int gen_01_dap1b_writer;
    Float64BufferRef gen_01_del1_buffer;
    Index gen_01_del1__maxdelay;
    Int gen_01_del1_sizemode;
    Index gen_01_del1_wrap;
    Int gen_01_del1_reader;
    Int gen_01_del1_writer;
    Float64BufferRef gen_01_dap2a_buffer;
    Index gen_01_dap2a__maxdelay;
    Int gen_01_dap2a_sizemode;
    Index gen_01_dap2a_wrap;
    Int gen_01_dap2a_reader;
    Int gen_01_dap2a_writer;
    Float64BufferRef gen_01_dap2b_buffer;
    Index gen_01_dap2b__maxdelay;
    Int gen_01_dap2b_sizemode;
    Index gen_01_dap2b_wrap;
    Int gen_01_dap2b_reader;
    Int gen_01_dap2b_writer;
    Float64BufferRef gen_01_del2_buffer;
    Index gen_01_del2__maxdelay;
    Int gen_01_del2_sizemode;
    Index gen_01_del2_wrap;
    Int gen_01_del2_reader;
    Int gen_01_del2_writer;
    number gen_01_lp_decay_1_value;
    number gen_01_lp_decay_2_value;
    number gen_01_lfo1_phase_value;
    number gen_01_lfo2_phase_value;
    number gen_01_noise_state_3_value;
    number gen_01_noise_state_4_value;
    UInt gen_01_noise_6_state[4] = { };
    UInt gen_01_noise_8_state[4] = { };
    number gen_01_dcblock_42_xm1;
    number gen_01_dcblock_42_ym1;
    number gen_01_dcblock_44_xm1;
    number gen_01_dcblock_44_ym1;
    bool gen_01_setupDone;
    number param_06_lastValue;
    number param_07_lastValue;
    signal globaltransport_tempo;
    signal globaltransport_state;
    number stackprotect_count;
    DataRef gen_01_ap1_bufferobj;
    DataRef gen_01_ap2_bufferobj;
    DataRef gen_01_ap3_bufferobj;
    DataRef gen_01_ap4_bufferobj;
    DataRef gen_01_dap1a_bufferobj;
    DataRef gen_01_dap1b_bufferobj;
    DataRef gen_01_del1_bufferobj;
    DataRef gen_01_dap2a_bufferobj;
    DataRef gen_01_dap2b_bufferobj;
    DataRef gen_01_del2_bufferobj;
    Index _voiceIndex;
    Int _noteNumber;
    Index isMuted;
    indexlist paramInitIndices;
    indexlist paramInitOrder;
    bool _isInitialized = false;
};

static PatcherInterface* createFreezeVerb()
{
    return new FreezeVerb<EXTERNALENGINE>();
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" PatcherFactoryFunctionPtr GetPatcherFactoryFunction()
#else
extern "C" PatcherFactoryFunctionPtr FreezeVerbFactoryFunction()
#endif
{
    return createFreezeVerb;
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" void SetLogger(Logger* logger)
#else
void FreezeVerbSetLogger(Logger* logger)
#endif
{
    console = logger;
}

} // end RNBO namespace

