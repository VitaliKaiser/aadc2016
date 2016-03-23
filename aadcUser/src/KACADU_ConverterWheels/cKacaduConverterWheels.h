/**
Copyright (c)
Audi Autonomous Driving Cup. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.  All advertising materials mentioning features or use of this software must display the following acknowledgement: “This product includes software developed by the Audi AG and its contributors for Audi Autonomous Driving Cup.”
4.  Neither the name of Audi nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY AUDI AG AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL AUDI AG OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


**********************************************************************
* $Author:: spiesra $  $Date:: 2015-05-13 08:29:07#$ $Rev:: 35003   $
**********************************************************************/

#ifndef _RPMFILTER_H_
#define _RPMFILTER_H_

#define OID_KACADU_ADTF_CONVERTER_WHEEL "adtf.KACADU.KACADU_converterWheels"


/*!
This filter calculates the speed of the vehicle and some distance measurements. It takes the wheel structs containing the direction and the interrupt counter of both wheels from the Arduino sensor as input values. The wheels have a small disk mounted at the inner side and a Transmissive  Encoder Sensor which detects the slots in the rotating disk. The detected slots are transmitted to a interrupt routine at a Arduino which counts the ticks and sends them periodically to ADTF.
The speed is calculated by the difference between the increasing tick counter in the incoming media samples and the corresponding time  difference between the media samples. For a correct calculation the wheel circumference has to be set to the correct value in the properties, the default value is 0.34m.
The output samples are triggered to the wheel right struct, so if no samples are received from that sensor no output samples are generated by this filter.
If the property Filtering enabled is set to true a first order filtering is applied to smooth the signals. The first order filter constant can also be set in the properties.

*/
class cKacaduConverterWheels : public adtf::cFilter
{
    ADTF_DECLARE_FILTER_VERSION(OID_KACADU_ADTF_CONVERTER_WHEEL, "KACADU Converter Wheels", OBJCAT_DataFilter, "KACADU Converter Wheels", 1, 0, 1, "BFFT GmbH and KACADU");

    /*! Input Pin for wheel struct*/
    cInputPin      m_oInputWheelLeft;

    /*! Input Pin for wheel struct*/
    cInputPin      m_oInputWheelRight;

    /*! output pin for the the speed of the wheels */
    cOutputPin m_oOutputCarSpeed;

    /*! output pin for the distance since the last sample */
    cOutputPin m_oOutputDistanceLastSample;

public:
    cKacaduConverterWheels(const tChar* __info);
    virtual ~cKacaduConverterWheels();

protected: // overwrites cFilter
    tResult Init(tInitStage eStage, __exception = NULL);
    tResult OnPinEvent(IPin* pSource, tInt nEventCode, tInt nParam1, tInt nParam2, IMediaSample* pMediaSample);
    tResult PropertyChanged(const tChar* strName);
private:
    /*!gets the actual time as a tTimeStamp */
    tTimeStamp GetTime();

    /*! calculate the speed from two timestamps
    @param currentTimeStamp the new and current timestamp
    @param lastTimeStamp the former timestamp
    @param ui32Ticks the ticks since last sample
    */
    tFloat32 calculateSpeed(const tUInt32 &ui32CurrentTimeStamp, const tUInt32 &ui32LastTimeStamp, const int &ui32Ticks=1);

    /* with this function all the samples (distance, overall distance, speed) on the output pins are transmitted
    */
    tResult TransmitSamples();

    /*! descriptor for wheel sensor data */
    cObjectPtr<IMediaTypeDescription> m_pDescriptionWheelDataLeft;
    /*! the id for the ui32WheelTach of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataLeftUi32WheelTach;
    /*! the id for the i8WheelDir of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataLeftI8WheelDir;
    /*! the id for the arduino time stamp of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataLeftArduinoTimestamp;
    /*! indicates if bufferIDs were set */
    tBool m_bIDsWheelDataLeftSet;

    /*! descriptor for wheel sensor data */
    cObjectPtr<IMediaTypeDescription> m_pDescriptionWheelDataRight;
    /*! the id for the ui32WheelTach of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataRightUi32WheelTach;
    /*! the id for the i8WheelDir of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataRightI8WheelDir;
    /*! the id for the arduino time stamp of the media description for input pin of the WheelData data */
    tBufferID m_szIDWheelDataRightArduinoTimestamp;
    /*! indicates if bufferIDs were set */
    tBool m_bIDsWheelDataRightSet;

    /*! descriptor for wheel sensor data */
    cObjectPtr<IMediaTypeDescription> m_pDescriptionOutputSpeed;
    /*! the id for the f32value of the media description for the actuator output pins */
    tBufferID m_szIDSpeedF32Value;
    /*! the id for the arduino time stamp of the media description for the actuator output input pins */
    tBufferID m_szIDSpeedArduinoTimestamp;
    /*! indicates if bufferIDs were set */
    tBool m_bIDsSpeedSet;

    /*! descriptor for sample distance data */
    cObjectPtr<IMediaTypeDescription> m_pDescriptionOutputSampleDistance;
    /*! the id for the f32value of the media description for the actuator output pins */
    tBufferID m_szIDSampleDistanceF32Value;
    /*! the id for the arduino time stamp of the media description for the actuator output input pins */
    tBufferID m_szIDSampleDistanceArduinoTimestamp;
    /*! indicates if bufferIDs were set */
    tBool m_bIDsSampleDistanceSet;


    /*! the timestamp of the last left wheel struct */
    tWheelData m_tLastStructLeft;

    /*! the timestamp of the last right wheel struct */
    tWheelData m_tLastStructRight;

    /*! the timestamp of the last left wheel struct */
    tWheelData m_tBeforeLastStructLeft;

    /*! the timestamp of the last right wheel struct */
    tWheelData m_tBeforeLastStructRight;

    /*! the last calculated speed of right wheel */
    tFloat32 m_f32LastCalculatedSpeedRight;

    /*! the last calculated speed of left wheel */
    tFloat32 m_f32LastCalculatedSpeedLeft;

    /*! holds the distance since the last sample */
    tFloat32 m_f32LastOverallDistance;

    /*! the wheel circumference in meter */
    tFloat32 m_f32wheelCircumference;

    /*! critical section for on pin event*/
    cCriticalSection m_oCritSectionTransmit;


    /*! first sample was received from left wheel */
    tBool m_bfirstSampleReceivedLeftWheel;

    /*! first sample was received from right wheel */
    tBool m_bfirstSampleReceivedRightWheel;

    int m_ticksLeft;
    int m_ticksRight;

    //Diff of Ticks since last sample. We assume, that always a left and a right media sample will come.
    int m_TicksDiffRight;
    int m_TicksDiffLeft;

};




//*************************************************************************************************

#endif // _RPMFILTER_H_

