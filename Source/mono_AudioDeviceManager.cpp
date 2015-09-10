#include "mono_AudioDeviceManager.h"

//==============================================================================
//==============================================================================
//==============================================================================
COLD mono_AudioDeviceManager::mono_AudioDeviceManager() noexcept
:
main_input_thru( false ),
                 cc_input_thru( false ),
                 use_main_input_as_cc( false ),

                 cc_input_callback( new MidiInputCallback_CC(this) ),
                 note_input_callback( new MidiInputCallback_NOTES(this) ),

                 midi_thru_output(nullptr),
                 midi_feedback_output(nullptr)
{
    sample_rate_changed(0);
}

COLD mono_AudioDeviceManager::~mono_AudioDeviceManager() noexcept
{
    StringArray devices = get_available_in_ports();
    for( int i = 0 ; i != devices.size() ; ++i )
    {
        removeMidiInputCallback( devices[i], cc_input_callback );
        removeMidiInputCallback( devices[i], note_input_callback );
    }

    delete cc_input_callback;
    delete note_input_callback;

    if( midi_thru_output )
    {
        delete midi_thru_output;
        midi_thru_output = nullptr;
    }
    if( midi_feedback_output )
    {
        delete midi_feedback_output;
        midi_feedback_output = nullptr;
    }
}

//==============================================================================
COLD void mono_AudioDeviceManager::sample_rate_changed( double /* old_sr_ */ ) noexcept
{
    cc_feedback_collector.reset(sample_rate);
    thru_collector.reset(sample_rate);
    cc_input_collector.reset(sample_rate);
    note_input_collector.reset(sample_rate);
}

//==============================================================================
COLD bool mono_AudioDeviceManager::save_to( XmlElement* xml_ ) const noexcept
{
    XmlElement* audio_device_setup( AudioDeviceManager::createStateXml() );

    bool success = false;
    if( audio_device_setup and xml_  )
    {
        // AUDIO
        xml_->addChildElement( audio_device_setup );

        // INPUT
        xml_->setAttribute( "noteInputDeviceName", note_input_callback->get_device_name() );
        xml_->setAttribute( "ccInputDeviceName", cc_input_callback->get_device_name() );

        // OUTPU
        xml_->setAttribute( "thruOutputDeviceName", midi_thru_name );
        xml_->setAttribute( "feedbackOutputDeviceName", midi_feedback_name );

        success = true;
    }
    return success;
}
COLD String mono_AudioDeviceManager::read_from( const XmlElement* xml_ ) noexcept
{
    String error;
    if( xml_ )
    {
#ifdef IS_STANDALONE
        // AUDIO
        {
            const OwnedArray<AudioIODeviceType>& types = getAvailableDeviceTypes();
            error = AudioDeviceManager::initialise
            (
                0,2,
                xml_->getChildByName("DEVICESETUP"),
                true
            );
        }
#endif
        // INPUT
        {
            String note_device = xml_->getStringAttribute( "noteInputDeviceName", "" );
            if( note_device != "" )
            {
                open_in_port( INPUT_ID::NOTES, note_device );
            }
        }
        {
            String cc_device = xml_->getStringAttribute( "ccInputDeviceName", "" );
            if( cc_device != "" )
            {
                open_in_port( INPUT_ID::CC, cc_device );
            }
        }

        // OUTPU
        {
            String thru_device = xml_->getStringAttribute( "thruOutputDeviceName", "" );
            if( thru_device != "" )
            {
                open_out_port( OUTPUT_ID::THRU, thru_device );
            }
        }
        {
            String feedback_device = xml_->getStringAttribute( "feedbackOutputDeviceName", "" );
            if( feedback_device != "" )
            {
                open_out_port( OUTPUT_ID::FEEDBACK, feedback_device );
            }
        }
    }
    else
    {
        error = "DEVICE XML INVALID";
    }

    return error;
}
COLD void mono_AudioDeviceManager::save() const noexcept
{
    File folder = File::getSpecialLocation(File::SpecialLocationType::ROOT_FOLDER);
    folder = File(folder.getFullPathName()+PROJECT_FOLDER);
    if( folder.createDirectory() )
    {
        File device_file( File( folder.getFullPathName() + String("/") + "devices.mcfg") );

        XmlElement xml("DEVICES-1.0");
        if( save_to( &xml ) )
        {
            xml.writeToFile(device_file,"");
        }
    }
}
COLD String mono_AudioDeviceManager::read() noexcept
{
    std::cout << "MONIQUE: init audio" << std::endl;

    File folder = File::getSpecialLocation(File::SpecialLocationType::ROOT_FOLDER);
    File device_file = File(folder.getFullPathName()+PROJECT_FOLDER+String("devices.mcfg"));

    String error;
    if( ScopedPointer<XmlElement> xml = XmlDocument( device_file ).getDocumentElement() )
    {
        if( xml->hasTagName("DEVICES-1.0") )
        {
            error = read_from( xml );
        }
        else
        {
            error = "WRONG DEVICE FILE VERSION";
        }
    }
    else
    {
        error = "CAN'T READ DEVICE FILE";
    }

    return error;
}

//==============================================================================
//==============================================================================
//==============================================================================
void mono_AudioDeviceManager::AdvancedMidiInputCallback::set_device_name(const String& name_) noexcept
{
    device_name = name_;
}
const String& mono_AudioDeviceManager::AdvancedMidiInputCallback::get_device_name() const noexcept
{
    return device_name;
}

//==============================================================================
void mono_AudioDeviceManager::MidiInputCallback_CC::handleIncomingMidiMessage(MidiInput*, const MidiMessage& message)
{
    manager->collect_incoming_midi_messages( INPUT_ID::CC, message );
}

//==============================================================================
void mono_AudioDeviceManager::MidiInputCallback_NOTES::handleIncomingMidiMessage(MidiInput*, const MidiMessage& message)
{
    manager->collect_incoming_midi_messages( INPUT_ID::NOTES, message );
}

//==============================================================================
//==============================================================================
//==============================================================================
void mono_AudioDeviceManager::collect_incoming_midi_messages(mono_AudioDeviceManager::INPUT_ID input_id_, const MidiMessage& midi_message_) noexcept
{
    switch( input_id_ )
    {
    case INPUT_ID::CC :
    {
        if( midi_message_.isController() )
        {
            cc_input_collector.addMessageToQueue( midi_message_ );
        }
    }
    break;
    case INPUT_ID::NOTES :
    {
#ifdef IS_STANDALONE
        if( midi_message_.isMidiClock() )
        {
            note_input_collector.addMessageToQueue( midi_message_ );
        }
        else if( midi_message_.isMidiStart() )
        {
            note_input_collector.addMessageToQueue( midi_message_ );
        }
        else if( midi_message_.isMidiStop() )
        {
            note_input_collector.addMessageToQueue( midi_message_ );
        }
        else if( midi_message_.isMidiContinue() )
        {
            note_input_collector.addMessageToQueue( midi_message_ );
        }
        else // IF
#endif
            // ELSE IF!!!!
            if( midi_message_.isNoteOnOrOff() )
            {
                note_input_collector.addMessageToQueue( midi_message_ );
                if( main_input_thru )
                {
                    thru_collector.addMessageToQueue( midi_message_ );
                }
            }
            else if( use_main_input_as_cc and midi_message_.isController() )
            {
                cc_input_collector.addMessageToQueue( midi_message_ );
                if( main_input_thru )
                {
                    thru_collector.addMessageToQueue( midi_message_ );
                }
            }
    }
    break;
    }
}

//==============================================================================
mono_AudioDeviceManager::AdvancedMidiInputCallback* mono_AudioDeviceManager::get_input_device_callback(mono_AudioDeviceManager::INPUT_ID input_id_) const noexcept
{
    switch( input_id_ )
    {
    case INPUT_ID::CC :
        return cc_input_callback;
    case INPUT_ID::NOTES :
        return note_input_callback;
    }
}

//==============================================================================
StringArray mono_AudioDeviceManager::get_available_in_ports() const noexcept
{
    return MidiInput::getDevices();
}
void mono_AudioDeviceManager::open_in_port(mono_AudioDeviceManager::INPUT_ID input_id_, const String& device_name_) noexcept
{
    close_in_port( input_id_ );

    setMidiInputEnabled( device_name_, true );

    AdvancedMidiInputCallback*midi_callback( get_input_device_callback( input_id_ ) );
    addMidiInputCallback( device_name_, midi_callback );
    midi_callback->set_device_name( device_name_ );
}
void mono_AudioDeviceManager::close_in_port(mono_AudioDeviceManager::INPUT_ID input_id_) noexcept
{
    AdvancedMidiInputCallback*const midi_callback( get_input_device_callback( input_id_ ) );
    StringArray devices = get_available_in_ports();
    for( int i = 0 ; i != devices.size() ; ++i )
    {
        removeMidiInputCallback( devices[i], midi_callback );
    }
    midi_callback->set_device_name( "" );
}
String mono_AudioDeviceManager::get_selected_in_device(mono_AudioDeviceManager::INPUT_ID input_id_) const noexcept
{
    return get_input_device_callback( input_id_ )->get_device_name();
}

//==============================================================================
void mono_AudioDeviceManager::send_thru_messages(MidiBuffer& midi_messages_, int pos_) noexcept
{
    if( midi_thru_output )
    {
        midi_thru_output->sendBlockOfMessages( midi_messages_, 1, sample_rate );
    }
}
void mono_AudioDeviceManager::send_feedback_messages(MidiBuffer& midi_messages_, int pos_) noexcept
{
    if( midi_feedback_output )
    {
        midi_feedback_output->sendBlockOfMessages( midi_messages_, 1, sample_rate );
    }
}

//==============================================================================
MidiOutput* mono_AudioDeviceManager::get_output_device(mono_AudioDeviceManager::OUTPUT_ID output_id_) const noexcept
{
    switch( output_id_ )
    {
    case OUTPUT_ID::THRU :
        return midi_thru_output;
    case OUTPUT_ID::FEEDBACK :
        return midi_feedback_output;
    }
}
StringArray mono_AudioDeviceManager::get_available_out_ports() const noexcept
{
    return MidiOutput::getDevices();
}
void mono_AudioDeviceManager::close_out_port( OUTPUT_ID output_id_ ) noexcept
{
    switch( output_id_ )
    {
    case OUTPUT_ID::FEEDBACK :
        if( midi_feedback_output )
        {
            midi_feedback_output->startBackgroundThread();
            delete midi_feedback_output;
            midi_feedback_output = nullptr;
        }
        midi_feedback_name = "";
        break;
    case OUTPUT_ID::THRU :
        if( midi_thru_output )
        {
            midi_thru_output->startBackgroundThread();
            delete midi_thru_output;
            midi_thru_output = nullptr;
        }
        midi_thru_name = "";
        break;
    }
}
bool mono_AudioDeviceManager::open_out_port(mono_AudioDeviceManager::OUTPUT_ID output_id_, const String& device_name_) noexcept
{
    // CLOSE
    close_out_port( output_id_ );

    // OPEN
    MidiOutput* output = MidiOutput::openDevice( get_available_out_ports().indexOf( device_name_ ) );
    if( output )
    {
        switch( output_id_ )
        {
        case OUTPUT_ID::FEEDBACK :
            midi_feedback_output = output;
            midi_feedback_name = device_name_;
            break;
        case OUTPUT_ID::THRU :
            midi_thru_output = output;
            midi_thru_name = device_name_;
            break;
        }

        output->startBackgroundThread();
    }

    return output;
}
String mono_AudioDeviceManager::get_selected_out_device(mono_AudioDeviceManager::OUTPUT_ID output_id_) const noexcept
{
    switch( output_id_ )
    {
    case OUTPUT_ID::FEEDBACK :
        return midi_feedback_name;
        break;
    case OUTPUT_ID::THRU :
        return midi_thru_name;
        break;
    }
}

