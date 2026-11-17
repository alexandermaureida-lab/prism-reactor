#include "TransportBar.h"

using namespace juce;

namespace
{
const Colour kAccent (0xff4de2c5);
const Colour kRec (0xffff4d6a);
String mmss (double t)
{
    const int s = (int) t;
    return String (s / 60) + ":" + String (s % 60).paddedLeft ('0', 2);
}
}

TransportBar::TransportBar (PrismProcessor& p) : proc (p)
{
    for (auto* b : { &openBtn, &playBtn, &recBtn })
    {
        b->setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.05f));
        b->setColour (TextButton::textColourOffId, Colours::white.withAlpha (0.85f));
        addAndMakeVisible (b);
    }
    openBtn.onClick = [this] { openFile(); };
    playBtn.onClick = [this] { togglePlay(); };
    recBtn.setColour (TextButton::textColourOffId, kRec);
    recBtn.onClick = [this]
    {
        if (proc.isRecording())
        {
            if (proc.stopRecording().existsAsFile() && onRecordingStopped) onRecordingStopped();
        }
        else if (! proc.startRecording())
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Record", "Could not start recording. Start the audio first.");
        else recStartMs = Time::currentTimeMillis();
    };

    sourceBox.addItem ("Live input", 1);
    sourceBox.addItem ("Audio file", 2);
    sourceBox.setSelectedId (1, dontSendNotification);
    sourceBox.onChange = [this]
    {
        if (sourceBox.getSelectedId() == 2)
        {
            if (! proc.hasAudioFile()) { sourceBox.setSelectedId (1, dontSendNotification); openFile(); }
            else proc.setUseFile (true);
        }
        else { proc.filePlay (false); proc.setUseFile (false); }
    };
    addAndMakeVisible (sourceBox);

    pos.setSliderStyle (Slider::LinearBar);
    pos.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    pos.setColour (Slider::trackColourId, kAccent.withAlpha (0.35f));
    pos.setRange (0.0, 1.0, 0.0);
    pos.onValueChange = [this] { if (pos.isMouseButtonDown()) proc.fileSeek (pos.getValue()); };
    addAndMakeVisible (pos);

    timeLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.75f));
    timeLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (timeLabel);
    nameLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.5f));
    nameLabel.setFont (Font (FontOptions (11.0f)));
    addAndMakeVisible (nameLabel);
    startTimerHz (15);
}

void TransportBar::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b1119));
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void TransportBar::resized()
{
    auto a = getLocalBounds().reduced (8, 6);
    sourceBox.setBounds (a.removeFromLeft (110)); a.removeFromLeft (6);
    openBtn.setBounds (a.removeFromLeft (92)); a.removeFromLeft (6);
    playBtn.setBounds (a.removeFromLeft (62)); a.removeFromLeft (10);
    recBtn.setBounds (a.removeFromRight (104)); a.removeFromRight (10);
    timeLabel.setBounds (a.removeFromRight (86));
    nameLabel.setBounds (a.removeFromRight (150));
    pos.setBounds (a.reduced (0, 3));
}

void TransportBar::openFile()
{
    chooser = std::make_unique<FileChooser> ("Choose an audio file", File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.caf");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        if (proc.loadAudioFile (f)) { proc.setUseFile (true); proc.filePlay (true); }
        else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Audio", "That file could not be read.");
    });
}

void TransportBar::togglePlay()
{
    if (! proc.hasAudioFile()) { openFile(); return; }
    proc.filePlay (! proc.fileIsPlaying());
}

void TransportBar::timerCallback()
{
    const bool hasFile = proc.hasAudioFile();
    const double len = proc.fileLength();
    if (hasFile && len > 0.0)
    {
        if (std::abs (pos.getMaximum() - len) > 0.01) pos.setRange (0.0, len, 0.0);
        if (! pos.isMouseButtonDown()) pos.setValue (proc.fileTime(), dontSendNotification);
        timeLabel.setText (mmss (proc.fileTime()) + " / " + mmss (len), dontSendNotification);
        nameLabel.setText (proc.getLoadedAudioFile().getFileName(), dontSendNotification);
    }
    else { timeLabel.setText ("", dontSendNotification); nameLabel.setText ("no file loaded", dontSendNotification); }

    playBtn.setButtonText (proc.fileIsPlaying() ? "PAUSE" : "PLAY");
    const int want = proc.isUsingFile() ? 2 : 1;
    if (sourceBox.getSelectedId() != want) sourceBox.setSelectedId (want, dontSendNotification);

    if (proc.isRecording())
        recBtn.setButtonText ("STOP " + mmss ((double) (Time::currentTimeMillis() - recStartMs) / 1000.0));
    else recBtn.setButtonText ("REC AUDIO");
}
