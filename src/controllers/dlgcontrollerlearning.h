/**
* @file dlgcontrollerlearning.h
* @author Sean M. Pappalardo  spappalardo@mixxx.org
* @date Thu 12 Apr 2012
* @brief The controller mapping learning wizard
*
*/
#ifndef DLGCONTROLLERLEARNING_H
#define DLGCONTROLLERLEARNING_H

#include <QDialog>
#include <QList>
#include <QString>
#include <QTimer>

#include "controllers/ui_dlgcontrollerlearning.h"
#include "controllers/controlpickermenu.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/hid/hidcontroller.h"
#include "controllers/bulk/bulkcontroller.h"
#include "controllers/midi/midimessage.h"
#include "controllers/controller.h"
#include "controllers/controllervisitor.h"
#include "configobject.h"

class ControllerPreset;

class DlgControllerLearning : public QDialog,
                              public ControllerVisitor,
                              public Ui::DlgControllerLearning {
    Q_OBJECT
  public:
    DlgControllerLearning(QWidget *parent, Controller *controller);
    virtual ~DlgControllerLearning();

    void visit(MidiController* pController);
    void visit(HidController* pController);
    void visit(BulkController* pController);

  signals:
    void learnTemporaryInputMappings(const MidiInputMappings& mappings);
    void clearTemporaryInputMappings();
    void commitTemporaryInputMappings();

    // Used to notify DlgPrefController that we have learned some new input
    // mappings.
    void inputMappingsLearned(const MidiInputMappings& mappings);

    void startLearning();
    void stopLearning();
    void listenForClicks();
    void stopListeningForClicks();

  public slots:
    // Triggered when the user picks a control from the menu.
    void controlPicked(ConfigKey control);
    // Triggered when user clicks a control from the GUI
    void controlClicked(ControlObject* pControl);

    void slotMessageReceived(unsigned char status,
                             unsigned char control,
                             unsigned char value);

    void slotTimerExpired();
    void slotUndo();
    void slotMidiOptionsChanged();

  private slots:
    void showControlMenu();

  private:
    void loadControl(const ConfigKey& key, QString description);
    void resetMapping(bool commit);

    MidiController* m_pMidiController;
    ControlPickerMenu m_controlPickerMenu;
    ConfigKey m_currentControl;
    QString m_currentDescription;
    bool m_messagesLearned;
    QTimer m_lastMessageTimer;
    QList<QPair<MidiKey, unsigned char> > m_messages;
    MidiInputMappings m_mappings;
};

#endif
