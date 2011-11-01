/*
  ==============================================================================

    dRowAudio_AudioThumbnailImage.cpp
    Created: 9 Jul 2011 7:35:03pm
    Author:  David Rowland

  ==============================================================================
*/

BEGIN_JUCE_NAMESPACE

AudioThumbnailImage::AudioThumbnailImage (AudioFilePlayer* sourceToBeUsed,
                                          TimeSliceThread& backgroundThread_,
                                          AudioThumbnailCache* cacheToUse,
                                          AudioThumbnail* thumbnailToUse,
                                          int sourceSamplesPerThumbnailSample_)
    : filePlayer            (sourceToBeUsed),
	  currentSampleRate     (44100.0),
      oneOverSampleRate     (1.0),
      backgroundThread      (backgroundThread_),
      audioThumbnailCache   (cacheToUse, (cacheToUse == nullptr) ? true : false),
      audioThumbnail        (thumbnailToUse, (thumbnailToUse == nullptr) ? true : false),
      sourceSamplesPerThumbnailSample (sourceSamplesPerThumbnailSample_),
      lastTimeDrawn         (0.0),
      resolution            (3.0)
{
    jassert (filePlayer != nullptr);
    
    waveformImage = Image (Image::RGB, 1, 1, false);

	// instansiate the cache and the thumbnail if needed
	if (audioThumbnailCache == nullptr)
    {
        OptionalScopedPointer<AudioThumbnailCache> newCache (new AudioThumbnailCache (3), true);
		audioThumbnailCache = newCache;
	}
    if (thumbnailToUse == nullptr)
    {
        OptionalScopedPointer<AudioThumbnail> newThumbnail (new AudioThumbnail (sourceSamplesPerThumbnailSample,
                                                                                *filePlayer->getAudioFormatManager(),
                                                                                *audioThumbnailCache),
                                                            true);
        audioThumbnail = newThumbnail;
    }
	//audioThumbnail->addChangeListener (this);
    
	// register with the file player to recieve update messages
	filePlayer->addListener (this);
}

AudioThumbnailImage::~AudioThumbnailImage()
{
	filePlayer->removeListener (this);
    
    for (int i = 0; i < backgroundThread.getNumClients(); i++) 
    {
        if (backgroundThread.getClient (i) == this) 
        {
            backgroundThread.removeTimeSliceClient (this);
        }
    }
	//stopTimer ();
}

//====================================================================================
Image AudioThumbnailImage::getImageAtTime (double startTime, double duration)
{
    const int startPixel = roundToInt (startTime * oneOverFileLength * waveformImage.getWidth());
    const int numPixels = roundToInt (duration * oneOverFileLength * waveformImage.getWidth());
    
    return waveformImage.getClippedImage (Rectangle<int> (startPixel, 0, numPixels, waveformImage.getHeight()));
}

void AudioThumbnailImage::setResolution (double newResolution)
{
    resolution = newResolution;
    
    waveformImage.clear (waveformImage.getBounds(), Colours::black);
    lastTimeDrawn = 0.0;
    refreshWaveform();
}

//====================================================================================
//void AudioThumbnailImage::timerCallback ()
//{
//    refreshWaveform();
//}

int AudioThumbnailImage::useTimeSlice()
{
    refreshWaveform();
    
    return 100;
}

void AudioThumbnailImage::fileChanged (AudioFilePlayer *player)
{
	if (player == filePlayer)
	{
		currentSampleRate = filePlayer->getAudioFormatReaderSource()->getAudioFormatReader()->sampleRate;

        if (currentSampleRate > 0.0)
        {
            oneOverSampleRate = 1.0 / currentSampleRate;
            fileLength = filePlayer->getLengthInSeconds();
            oneOverFileLength = 1.0 / fileLength;
            
            const int imageWidth = filePlayer->getTotalLength() / sourceSamplesPerThumbnailSample;
            waveformImage = Image (Image::RGB, imageWidth, 100, true);

            waveformImage.clear (waveformImage.getBounds(), Colours::black);
            lastTimeDrawn = 0.0;
            
            File newFile (filePlayer->getPath());
            if (newFile.existsAsFile()) 
            {
                FileInputSource* fileInputSource = new FileInputSource (newFile);
                audioThumbnail->setSource (fileInputSource);
            }
            else 
            {
                audioThumbnail->setSource (nullptr);
            }
            
            listeners.call (&Listener::imageChanged, this);

            backgroundThread.addTimeSliceClient (this);
            if (! backgroundThread.isThreadRunning())
                backgroundThread.startThread (1);
            //startTimer (100);
        }
	}
}

//==============================================================================
void AudioThumbnailImage::addListener (AudioThumbnailImage::Listener* const listener)
{
    listeners.add (listener);
}

void AudioThumbnailImage::removeListener (AudioThumbnailImage::Listener* const listener)
{
    listeners.remove (listener);
}

//==============================================================================	
void AudioThumbnailImage::refreshWaveform()
{
    bool renderComplete = false;
    
	if (audioThumbnail->getNumSamplesFinished() > 0)
	{
        if (audioThumbnail->isFullyLoaded())
            renderComplete = true;

            const double endTime = audioThumbnail->getNumSamplesFinished() * oneOverSampleRate;
        if (lastTimeDrawn < 0.0)
            lastTimeDrawn -= (endTime - lastTimeDrawn) * 0.5; // overlap by 0.5
        const double startPixelX = roundToInt (lastTimeDrawn * oneOverFileLength * waveformImage.getWidth());
        const double numPixels = roundToInt ((endTime - lastTimeDrawn) * oneOverFileLength * waveformImage.getWidth());
        if (tempSectionImage.getWidth() < (numPixels * resolution))
        {
            tempSectionImage = Image (Image::RGB, 
                                      numPixels * resolution, waveformImage.getHeight(), 
                                      false);
        }

        Rectangle<int> rectangleToDraw (0, 0, 
                                        numPixels * resolution, waveformImage.getHeight());
        
        Graphics gTemp (tempSectionImage);
        tempSectionImage.clear(tempSectionImage.getBounds(), Colours::black);
        gTemp.setColour (Colours::green);
        audioThumbnail->drawChannel (gTemp, rectangleToDraw,
                                     lastTimeDrawn, endTime,
                                     0, 1.0f);
        lastTimeDrawn = endTime;
        
		Graphics g (waveformImage);
        g.drawImage (tempSectionImage,
                     startPixelX, 0, numPixels, waveformImage.getHeight(),
                     0, 0, numPixels * resolution, tempSectionImage.getHeight());
        
        listeners.call (&Listener::imageUpdated, this);
	}

    if (renderComplete)
    {
        backgroundThread.removeTimeSliceClient (this);

        listeners.call (&Listener::imageFinished, this);
    }
}

//==============================================================================

END_JUCE_NAMESPACE
