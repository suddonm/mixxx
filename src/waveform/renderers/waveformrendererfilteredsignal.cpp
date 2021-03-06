#include "waveformrendererfilteredsignal.h"

#include "waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"
#include "controlobjectthread.h"
#include "widget/wskincolor.h"
#include "trackinfoobject.h"
#include "widget/wwidget.h"
#include "defs.h"

WaveformRendererFilteredSignal::WaveformRendererFilteredSignal(
        WaveformWidgetRenderer* waveformWidgetRenderer)
    : WaveformRendererSignalBase(waveformWidgetRenderer) {
}

WaveformRendererFilteredSignal::~WaveformRendererFilteredSignal() {
}

void WaveformRendererFilteredSignal::onResize() {
    m_lowLines.resize(m_waveformRenderer->getWidth());
    m_midLines.resize(m_waveformRenderer->getWidth());
    m_highLines.resize(m_waveformRenderer->getWidth());
}

void WaveformRendererFilteredSignal::onSetup(const QDomNode& node) {
    Q_UNUSED(node);
}

void WaveformRendererFilteredSignal::draw(QPainter* painter,
                                          QPaintEvent* /*event*/) {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();
    if (!trackInfo) {
        return;
    }

    const Waveform* waveform = trackInfo->getWaveform();
    if (waveform == NULL) {
        return;
    }

    const int dataSize = waveform->getDataSize();
    if (dataSize <= 1) {
        return;
    }

    const WaveformData* data = waveform->data();
    if (data == NULL) {
        return;
    }

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing, false);
    painter->setRenderHints(QPainter::HighQualityAntialiasing, false);
    painter->setRenderHints(QPainter::SmoothPixmapTransform, false);
    painter->setWorldMatrixEnabled(false);
    painter->resetTransform();

    const double firstVisualIndex = m_waveformRenderer->getFirstDisplayedPosition() * dataSize;
    const double lastVisualIndex = m_waveformRenderer->getLastDisplayedPosition() * dataSize;

    // Represents the # of waveform data points per horizontal pixel.
    const double gain = (lastVisualIndex - firstVisualIndex) /
            (double)m_waveformRenderer->getWidth();

    // Per-band gain from the EQ knobs.
    float lowGain(1.0), midGain(1.0), highGain(1.0), allGain(1.0);
    if (m_pLowFilterControlObject &&
            m_pMidFilterControlObject &&
            m_pHighFilterControlObject) {
        lowGain = m_pLowFilterControlObject->get();
        midGain = m_pMidFilterControlObject->get();
        highGain = m_pHighFilterControlObject->get();
    }
    allGain = m_waveformRenderer->getGain();

    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    allGain *= factory->getVisualGain(::WaveformWidgetFactory::All);
    lowGain *= factory->getVisualGain(WaveformWidgetFactory::Low);
    midGain *= factory->getVisualGain(WaveformWidgetFactory::Mid);
    highGain *= factory->getVisualGain(WaveformWidgetFactory::High);

    const float halfHeight = (float)m_waveformRenderer->getHeight()/2.0;

    const float heightFactor = m_alignment == Qt::AlignCenter
            ? allGain*halfHeight/255.0
            : allGain*m_waveformRenderer->getHeight()/255.0;

    //draw reference line
    if (m_alignment == Qt::AlignCenter) {
        painter->setPen(m_axesColor);
        painter->drawLine(0,halfHeight,m_waveformRenderer->getWidth(),halfHeight);
    }

    int actualLowLineNumber = 0;
    int actualMidLineNumber = 0;
    int actualHighLineNumber = 0;

    for (int x = 0; x < m_waveformRenderer->getWidth(); ++x) {
        // Width of the x position in visual indices.
        const double xSampleWidth = gain * x;

        // Effective visual index of x
        const double xVisualSampleIndex = xSampleWidth + firstVisualIndex;

        // Our current pixel (x) corresponds to a number of visual samples
        // (visualSamplerPerPixel) in our waveform object. We take the max of
        // all the data points on either side of xVisualSampleIndex within a
        // window of 'maxSamplingRange' visual samples to measure the maximum
        // data point contained by this pixel.
        double maxSamplingRange = gain / 2.0;

        // Since xVisualSampleIndex is in visual-samples (e.g. R,L,R,L) we want
        // to check +/- maxSamplingRange frames, not samples. To do this, divide
        // xVisualSampleIndex by 2. Since frames indices are integers, we round
        // to the nearest integer by adding 0.5 before casting to int.
        int visualFrameStart = int(xVisualSampleIndex / 2.0 - maxSamplingRange + 0.5);
        int visualFrameStop = int(xVisualSampleIndex / 2.0 + maxSamplingRange + 0.5);

        // If the entire sample range is off the screen then don't calculate a
        // point for this pixel.
        const int lastVisualFrame = dataSize / 2 - 1;
        if (visualFrameStop < 0 || visualFrameStart > lastVisualFrame) {
            continue;
        }

        // We now know that some subset of [visualFrameStart, visualFrameStop]
        // lies within the valid range of visual frames. Clamp
        // visualFrameStart/Stop to within [0, lastVisualFrame].
        visualFrameStart = math_max(math_min(lastVisualFrame, visualFrameStart), 0);
        visualFrameStop = math_max(math_min(lastVisualFrame, visualFrameStop), 0);

        int visualIndexStart = visualFrameStart * 2;
        int visualIndexStop = visualFrameStop * 2;

        // if (x == m_waveformRenderer->getWidth() / 2) {
        //     qDebug() << "audioVisualRatio" << waveform->getAudioVisualRatio();
        //     qDebug() << "visualSampleRate" << waveform->getVisualSampleRate();
        //     qDebug() << "audioSamplesPerVisualPixel" << waveform->getAudioSamplesPerVisualSample();
        //     qDebug() << "visualSamplePerPixel" << visualSamplePerPixel;
        //     qDebug() << "xSampleWidth" << xSampleWidth;
        //     qDebug() << "xVisualSampleIndex" << xVisualSampleIndex;
        //     qDebug() << "maxSamplingRange" << maxSamplingRange;;
        //     qDebug() << "Sampling pixel " << x << "over [" << visualIndexStart << visualIndexStop << "]";
        // }

        unsigned char maxLow[2] = {0, 0};
        unsigned char maxMid[2] = {0, 0};
        unsigned char maxHigh[2] = {0, 0};

        for (int i = visualIndexStart;
             i >= 0 && i + 1 < dataSize && i + 1 <= visualIndexStop; i += 2) {
            const WaveformData& waveformData = *(data + i);
            const WaveformData& waveformDataNext = *(data + i + 1);
            maxLow[0] = math_max(maxLow[0], waveformData.filtered.low);
            maxLow[1] = math_max(maxLow[1], waveformDataNext.filtered.low);
            maxMid[0] = math_max(maxMid[0], waveformData.filtered.mid);
            maxMid[1] = math_max(maxMid[1], waveformDataNext.filtered.mid);
            maxHigh[0] = math_max(maxHigh[0], waveformData.filtered.high);
            maxHigh[1] = math_max(maxHigh[1], waveformDataNext.filtered.high);
        }

        if (maxLow[0] && maxLow[1]) {
            switch (m_alignment) {
                case Qt::AlignBottom :
                    m_lowLines[actualLowLineNumber].setLine(
                        x, m_waveformRenderer->getHeight(),
                        x, m_waveformRenderer->getHeight() - (int)(heightFactor*lowGain*(float)math_max(maxLow[0],maxLow[1])));
                    break;
                case Qt::AlignTop :
                    m_lowLines[actualLowLineNumber].setLine(
                        x, 0,
                        x, (int)(heightFactor*lowGain*(float)math_max(maxLow[0],maxLow[1])));
                    break;
                default :
                    m_lowLines[actualLowLineNumber].setLine(
                        x, (int)(halfHeight-heightFactor*(float)maxLow[0]*lowGain),
                        x, (int)(halfHeight+heightFactor*(float)maxLow[1]*lowGain));
                    break;
            }
            actualLowLineNumber++;
        }
        if (maxMid[0] && maxMid[1]) {
            switch (m_alignment) {
                case Qt::AlignBottom :
                    m_midLines[actualMidLineNumber].setLine(
                        x, m_waveformRenderer->getHeight(),
                        x, m_waveformRenderer->getHeight() - (int)(heightFactor*midGain*(float)math_max(maxMid[0],maxMid[1])));
                    break;
                case Qt::AlignTop :
                    m_midLines[actualMidLineNumber].setLine(
                        x, 0,
                        x, (int)(heightFactor*midGain*(float)math_max(maxMid[0],maxMid[1])));
                    break;
                default :
                    m_midLines[actualMidLineNumber].setLine(
                        x, (int)(halfHeight-heightFactor*(float)maxMid[0]*midGain),
                        x, (int)(halfHeight+heightFactor*(float)maxMid[1]*midGain));
                    break;
            }
            actualMidLineNumber++;
        }
        if (maxHigh[0] && maxHigh[1]) {
            switch (m_alignment) {
                case Qt::AlignBottom :
                    m_highLines[actualHighLineNumber].setLine(
                        x, m_waveformRenderer->getHeight(),
                        x, m_waveformRenderer->getHeight() - (int)(heightFactor*highGain*(float)math_max(maxHigh[0],maxHigh[1])));
                    break;
                case Qt::AlignTop :
                    m_highLines[actualHighLineNumber].setLine(
                        x, 0,
                        x, (int)(heightFactor*highGain*(float)math_max(maxHigh[0],maxHigh[1])));
                    break;
                default :
                    m_highLines[actualHighLineNumber].setLine(
                        x, (int)(halfHeight-heightFactor*(float)maxHigh[0]*highGain),
                        x, (int)(halfHeight+heightFactor*(float)maxHigh[1]*highGain));
                    break;
            }
            actualHighLineNumber++;
        }
    }

    painter->setPen(QPen(QBrush(m_pColors->getLowColor()), 1));
    if (m_pLowKillControlObject && m_pLowKillControlObject->get() == 0.0) {
       painter->drawLines(&m_lowLines[0], actualLowLineNumber);
    }
    painter->setPen(QPen(QBrush(m_pColors->getMidColor()), 1));
    if (m_pMidKillControlObject && m_pMidKillControlObject->get() == 0.0) {
        painter->drawLines(&m_midLines[0], actualMidLineNumber);
    }
    painter->setPen(QPen(QBrush(m_pColors->getHighColor()), 1));
    if (m_pHighKillControlObject && m_pHighKillControlObject->get() == 0.0) {
        painter->drawLines(&m_highLines[0], actualHighLineNumber);
    }

    painter->restore();
}
