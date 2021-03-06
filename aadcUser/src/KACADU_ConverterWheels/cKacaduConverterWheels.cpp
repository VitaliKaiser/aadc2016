/**
Copyright (c)
Audi Autonomous Driving Cup. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.  All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed by the Audi AG and its contributors for Audi Autonomous Driving Cup.
4.  Neither the name of Audi nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY AUDI AG AND CONTRIBUTORS AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL AUDI AG OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


**********************************************************************
* $Author:: spiesra $  $Date:: 2015-05-13 08:29:07#$ $Rev:: 35003   $
**********************************************************************/


// arduinofilter.cpp : Definiert die exportierten Funktionen für die DLL-Anwendung.
//
#include "stdafx.h"
#include "cKacaduConverterWheels.h"

#include <iostream>

#define CW_SLOT_COUNT 60.f
#define CW_ERROR_DIFFERENCE_SIDES 0.30f
#define CW_MIN_LIMIT_IGNORE 0.01f

ADTF_FILTER_PLUGIN("KACADU Converter Wheels", OID_KACADU_ADTF_CONVERTER_WHEEL, cKacaduConverterWheels)

cKacaduConverterWheels::cKacaduConverterWheels(const tChar* __info) : cFilter(__info)
{
    SetPropertyFloat("Wheel circumference",0.34);
    SetPropertyFloat("Wheel circumference" NSSUBPROP_REQUIRED, tTrue);
    SetPropertyStr("Wheel circumference" NSSUBPROP_DESCRIPTION, "Set the wheel circumference in meter here");
}

cKacaduConverterWheels::~cKacaduConverterWheels()
{
}

tResult cKacaduConverterWheels::Init(tInitStage eStage, __exception)
{
    RETURN_IF_FAILED(cFilter::Init(eStage, __exception_ptr))

            if (eStage == StageFirst)
    {
        cObjectPtr<IMediaDescriptionManager> pDescManager;
        RETURN_IF_FAILED(_runtime->GetObject(OID_ADTF_MEDIA_DESCRIPTION_MANAGER,IID_ADTF_MEDIA_DESCRIPTION_MANAGER,(tVoid**)&pDescManager,__exception_ptr));

        tChar const * strDescSignalValue = pDescManager->GetMediaDescription("tSignalValue");
        RETURN_IF_POINTER_NULL(strDescSignalValue);
        cObjectPtr<IMediaType> pTypeSignalValue = new cMediaType(0, 0, 0, "tSignalValue", strDescSignalValue,IMediaDescription::MDF_DDL_DEFAULT_VERSION);


        //get description for wheel sensors data pins
        tChar const * strDescWheelData = pDescManager->GetMediaDescription("tWheelData");
        RETURN_IF_POINTER_NULL(strDescWheelData);
        //get mediatype for wheeldata sensor data pins
        cObjectPtr<IMediaType> pTypeWheelData = new cMediaType(0, 0, 0, "tWheelData", strDescWheelData, IMediaDescription::MDF_DDL_DEFAULT_VERSION);


        //create pin for wheel left sensor data
        RETURN_IF_FAILED(m_oInputWheelLeft.Create("WheelLeft_Struct", pTypeWheelData, static_cast<IPinEventSink*> (this)));
        RETURN_IF_FAILED(RegisterPin(&m_oInputWheelLeft));

        //create pin for wheel right data
        RETURN_IF_FAILED(m_oInputWheelRight.Create("WheelRight_Struct", pTypeWheelData, static_cast<IPinEventSink*> (this)));
        RETURN_IF_FAILED(RegisterPin(&m_oInputWheelRight));


        //create pin for car speed
        RETURN_IF_FAILED(m_oOutputCarSpeed.Create("car_speed", pTypeSignalValue, static_cast<IPinEventSink*> (this)));
        RETURN_IF_FAILED(RegisterPin(&m_oOutputCarSpeed));

        //create pin for distance last sample
        RETURN_IF_FAILED(m_oOutputDistanceLastSample.Create("distance_lastSample", pTypeSignalValue, static_cast<IPinEventSink*> (this)));
        RETURN_IF_FAILED(RegisterPin(&m_oOutputDistanceLastSample));

        //get mediatype description for output
        RETURN_IF_FAILED(pTypeSignalValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionOutputSpeed));

        RETURN_IF_FAILED(pTypeSignalValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionOutputSampleDistance));

        //get mediatype description for wheel sensor data type
        RETURN_IF_FAILED(pTypeWheelData->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionWheelDataRight));

        RETURN_IF_FAILED(pTypeWheelData->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionWheelDataLeft));
    }
    else if (eStage == StageNormal)
    {
        m_f32wheelCircumference = static_cast<tFloat32>(GetPropertyFloat("wheel circumference"));
        m_bfirstSampleReceivedLeftWheel = tFalse;
        m_bfirstSampleReceivedRightWheel = tFalse;
    }
    else if(eStage == StageGraphReady)
    {
        m_bIDsWheelDataLeftSet = tFalse;
        m_bIDsWheelDataRightSet = tFalse;
        m_bIDsSpeedSet = tFalse;
        m_bIDsSampleDistanceSet = tFalse;

        m_f32LastCalculatedSpeedRight = 0;
        m_f32LastCalculatedSpeedLeft = 0;

        m_TicksDiffRight = 0;
        m_TicksDiffLeft = 0;

        m_ticksLeft = 0;
        m_ticksRight = 0;

        m_f32LastOverallDistance = 0;

    }

    RETURN_NOERROR;
}

tResult cKacaduConverterWheels::OnPinEvent(    IPin* pSource, tInt nEventCode, tInt nParam1, tInt nParam2, IMediaSample* pMediaSample)
{
    if (nEventCode == IPinEventSink::PE_MediaSampleReceived && pMediaSample != NULL)
    {
        if (pSource == &m_oInputWheelLeft)
        {
            // save the last struct to the struct beforeLast if it is not the first one
            if (m_bfirstSampleReceivedLeftWheel==tTrue)
            {
                m_tBeforeLastStructLeft = m_tLastStructLeft;
            }

            tUInt32 ui32Tach = 0;
            tInt8 i8Direction = 0;
            tUInt32 ui32Timestamp = 0;
            {   // focus for sample read lock
                // read-out the incoming Media Sample
                __adtf_sample_read_lock_mediadescription(m_pDescriptionWheelDataLeft,pMediaSample,pCoderInput);

                // get IDs
                if(!m_bIDsWheelDataLeftSet)
                {
                    pCoderInput->GetID("i8WheelDir",m_szIDWheelDataLeftI8WheelDir);
                    pCoderInput->GetID("ui32WheelTach", m_szIDWheelDataLeftUi32WheelTach);
                    pCoderInput->GetID("ui32ArduinoTimestamp", m_szIDWheelDataLeftArduinoTimestamp);
                    m_bIDsWheelDataLeftSet = tTrue;
                }

                //get values from media sample
                pCoderInput->Get(m_szIDWheelDataLeftUi32WheelTach, (tVoid*)&ui32Tach);
                pCoderInput->Get(m_szIDWheelDataLeftI8WheelDir, (tVoid*)&i8Direction);
                pCoderInput->Get(m_szIDWheelDataLeftArduinoTimestamp, (tVoid*)&ui32Timestamp);
            }
            // if it is the first sample stop here and set to true
            if (m_bfirstSampleReceivedLeftWheel==tFalse)
            {
                m_bfirstSampleReceivedLeftWheel = tTrue;

                m_tLastStructLeft.i8WheelDir = i8Direction;
                m_tLastStructLeft.ui32ArduinoTimestamp = ui32Timestamp;
                m_tLastStructLeft.ui32WheelTach = ui32Tach;
            }
            else
            {
                m_TicksDiffLeft = ui32Tach-m_tLastStructLeft.ui32WheelTach;
                if(i8Direction == 1)
                {
                   m_TicksDiffLeft *= -1;
                }
                 m_ticksLeft += m_TicksDiffLeft;
                m_f32LastCalculatedSpeedLeft = calculateSpeed(ui32Timestamp,m_tLastStructLeft.ui32ArduinoTimestamp,m_TicksDiffLeft);
//                std::cout<<"Speed Left:"<<m_f32LastCalculatedSpeedLeft<<std::endl;

                m_tLastStructLeft.i8WheelDir = i8Direction;
                m_tLastStructLeft.ui32ArduinoTimestamp = ui32Timestamp;
                m_tLastStructLeft.ui32WheelTach = ui32Tach;
            }

        }
        else if (pSource == &m_oInputWheelRight)
        {
            // save the last struct to the struct beforeLast if it is not the first one
            if (m_bfirstSampleReceivedRightWheel==tTrue)
            {
                m_tBeforeLastStructRight = m_tLastStructRight;
            }

            tUInt32 ui32Tach = 0;
            tInt8 i8Direction = 0;
            tUInt32 ui32Timestamp = 0;
            {   // focus for sample read lock
                // read-out the incoming Media Sample
                __adtf_sample_read_lock_mediadescription(m_pDescriptionWheelDataRight,pMediaSample,pCoderInput);


                // get IDs
                if(!m_bIDsWheelDataRightSet)
                {
                    pCoderInput->GetID("i8WheelDir",m_szIDWheelDataRightI8WheelDir);
                    pCoderInput->GetID("ui32WheelTach", m_szIDWheelDataRightUi32WheelTach);
                    pCoderInput->GetID("ui32ArduinoTimestamp", m_szIDWheelDataRightArduinoTimestamp);
                    m_bIDsWheelDataRightSet = tTrue;
                }
                //get values from media sample
                pCoderInput->Get(m_szIDWheelDataRightUi32WheelTach, (tVoid*)&ui32Tach);
                pCoderInput->Get(m_szIDWheelDataRightI8WheelDir, (tVoid*)&i8Direction);
                pCoderInput->Get(m_szIDWheelDataRightArduinoTimestamp, (tVoid*)&ui32Timestamp);
            }
            // if it is the first sample stop here and set to true
            if (m_bfirstSampleReceivedRightWheel==tFalse)
            {
                m_bfirstSampleReceivedRightWheel = tTrue;

                m_tLastStructRight.i8WheelDir = i8Direction;
                m_tLastStructRight.ui32ArduinoTimestamp = ui32Timestamp;
                m_tLastStructRight.ui32WheelTach = ui32Tach;
            }
            // doing the calculation and the transmit
            else
            {
                m_TicksDiffRight = ui32Tach-m_tLastStructRight.ui32WheelTach;
                if(i8Direction == 1)
                {
                    m_TicksDiffRight *= -1;
                }

                m_ticksRight += m_TicksDiffRight;

                m_f32LastCalculatedSpeedRight = calculateSpeed(ui32Timestamp,m_tLastStructRight.ui32ArduinoTimestamp,m_TicksDiffRight);

                m_tLastStructRight.i8WheelDir = i8Direction;
                m_tLastStructRight.ui32ArduinoTimestamp = ui32Timestamp;
                m_tLastStructRight.ui32WheelTach = ui32Tach;

                m_f32LastOverallDistance = (((m_TicksDiffLeft+m_TicksDiffRight)/2.0)/CW_SLOT_COUNT)*m_f32wheelCircumference;

                TransmitSamples();
            }
        }
    }
    RETURN_NOERROR;
}

tTimeStamp cKacaduConverterWheels::GetTime()
{
    return (_clock != NULL) ? _clock->GetTime () : cSystem::GetTime();
}

tFloat32 cKacaduConverterWheels::calculateSpeed(const tUInt32 &ui32CurrentTimeStamp, const tUInt32 &ui32LastTimeStamp, const int &ui32Ticks)
{
    // return if time difference is 0, if time difference is smaller than 0, if ticks are 0 or smaller 0
    if ((ui32CurrentTimeStamp-ui32LastTimeStamp==0) || (ui32Ticks==0)) return 0;
    //          circumference      SlotsInTimeDiff
    // speed =  -------------- *  -------------
    //           TotalSlots*          TimeDiff
    return (m_f32wheelCircumference/CW_SLOT_COUNT*static_cast<tFloat32>(ui32Ticks))/
            (static_cast<tFloat32>(ui32CurrentTimeStamp-ui32LastTimeStamp)/static_cast<tFloat32>(1e6));
}

tResult cKacaduConverterWheels::TransmitSamples()
{
    // enter critical section for transmitting
    __synchronized_obj(m_oCritSectionTransmit);

    // static variable for warning outputs to console
    static tInt32 i32WarningCounter = 0;

    // calculate the average of both wheel speeds
    tFloat32 f32speed = (m_f32LastCalculatedSpeedRight+m_f32LastCalculatedSpeedLeft)/2;

    if (fabs((m_f32LastCalculatedSpeedRight-m_f32LastCalculatedSpeedLeft))> fabs(m_f32LastCalculatedSpeedRight)*CW_ERROR_DIFFERENCE_SIDES)
    {
        if (m_f32LastCalculatedSpeedRight<CW_MIN_LIMIT_IGNORE)
        {
            f32speed = m_f32LastCalculatedSpeedLeft;
        }
        else if (m_f32LastCalculatedSpeedLeft<CW_MIN_LIMIT_IGNORE)
        {
            f32speed = m_f32LastCalculatedSpeedRight;

        }
        i32WarningCounter++;
        if (i32WarningCounter%200==0)
            LOG_WARNING(cString::Format("Wheel speed from left and right side are very different. Please check cables and connections! Right: %f, Left: %f, Result: %f",
                                        m_f32LastCalculatedSpeedRight,m_f32LastCalculatedSpeedLeft,f32speed));
    }


//    std::cout<<"Overall Dist Left:  "<<(m_ticksLeft/CW_SLOT_COUNT)*m_f32wheelCircumference<<std::endl;
//    std::cout<<"Overall Dist Right: "<<(m_ticksRight/CW_SLOT_COUNT)*m_f32wheelCircumference<<std::endl;

    //calculate the average of the arduino timestamp
    tUInt32 ui32arduinoTimestamp =(m_tLastStructLeft.ui32ArduinoTimestamp + m_tLastStructRight.ui32ArduinoTimestamp)/2;

    //create new media sample for speed
    cObjectPtr<IMediaSample> pMediaSampleSpeed;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pMediaSampleSpeed));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pDescriptionOutputSpeed->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pMediaSampleSpeed->AllocBuffer(nSize));


    {   // focus for sample write lock
        //write date to the media sample with the coder of the descriptor
        __adtf_sample_write_lock_mediadescription(m_pDescriptionOutputSpeed,pMediaSampleSpeed,pCoder);

        // set the ids if not already done
        if(!m_bIDsSpeedSet)
        {
            pCoder->GetID("f32Value", m_szIDSpeedF32Value);
            pCoder->GetID("ui32ArduinoTimestamp", m_szIDSpeedArduinoTimestamp);
            m_bIDsSpeedSet = tTrue;
        }

        pCoder->Set(m_szIDSpeedArduinoTimestamp, (tVoid*)&ui32arduinoTimestamp);
        pCoder->Set(m_szIDSpeedF32Value, (tVoid*)&f32speed);
        //std::cout<<"send speed: "<<f32speed<<" m/s"<<std::endl;
    }

    //transmit media sample over output pin
    pMediaSampleSpeed->SetTime(_clock->GetStreamTime());
    RETURN_IF_FAILED(m_oOutputCarSpeed.Transmit(pMediaSampleSpeed));



    //create new media sample for distance
    cObjectPtr<IMediaSample> pMediaSampleDistance;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pMediaSampleDistance));

    //allocate memory with the size given by the descriptor
    m_pDescriptionOutputSampleDistance->GetMediaSampleSerializer(&pSerializer);
    nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pMediaSampleDistance->AllocBuffer(nSize));


    {   // focus for sample write lock
        //write date to the media sample with the coder of the descriptor
        __adtf_sample_write_lock_mediadescription(m_pDescriptionOutputSampleDistance,pMediaSampleDistance,pCoder);

        // set the ids if not already done
        if(!m_bIDsSampleDistanceSet)
        {
            pCoder->GetID("f32Value", m_szIDSampleDistanceF32Value);
            pCoder->GetID("ui32ArduinoTimestamp", m_szIDSampleDistanceArduinoTimestamp);
            m_bIDsSpeedSet = tTrue;
        }

        // set the values
        pCoder->Set(m_szIDSampleDistanceArduinoTimestamp, (tVoid*)&ui32arduinoTimestamp);
        pCoder->Set(m_szIDSampleDistanceF32Value, (tVoid*)&m_f32LastOverallDistance);
    }

    //transmit media sample over output pin
    pMediaSampleDistance->SetTime(_clock->GetStreamTime());
    RETURN_IF_FAILED(m_oOutputDistanceLastSample.Transmit(pMediaSampleDistance));

    RETURN_NOERROR;
}

tResult cKacaduConverterWheels::PropertyChanged(const tChar* strName)
{
    RETURN_NOERROR;
}


