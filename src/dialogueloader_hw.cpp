/*
 * $Id$
 */

#include <string>
#include <cstring>
#include <vector>

#include "context.h"
#include "conversation.h"
#include "dialogueloader_hw.h"
#include "player.h"
#include "savegame.h"
#include "u4file.h"
#include "utils.h"

using std::string;
using std::vector;

Response *hawkwindGetAdvice(const DynamicResponse *kw);
Response *hawkwindGetIntro(const DynamicResponse *dynResp);

/* Hawkwind text indexes */
#define HW_SPEAKONLYWITH 40
#define HW_RETURNWHEN 41
#define HW_ISREVIVED 42
#define HW_WELCOME 43
#define HW_GREETING1 44
#define HW_GREETING2 45
#define HW_PROMPT 46
#define HW_DEFAULT 49
#define HW_ALREADYAVATAR 50
#define HW_GOTOSHRINE 51
#define HW_BYE 52

vector<string> hawkwindText;

DialogueLoader* U4HWDialogueLoader::instance = DialogueLoader::registerLoader(new U4HWDialogueLoader, "application/x-u4hwtlk");

/**
 * A special case dialogue loader for Hawkwind.
 */
Dialogue* U4HWDialogueLoader::load(void *source) {
    U4FILE *avatar = u4fopen("avatar.exe");
    if (!avatar)
        return NULL;

    hawkwindText = u4read_stringtable(avatar, 74729, 53);

    Dialogue *dlg = new Dialogue();
    dlg->setTurnAwayProb(0);

    dlg->setName("Hawkwind");
    dlg->setPronoun("He");
    dlg->setPrompt(hawkwindText[HW_PROMPT]);
    Response *intro = new DynamicResponse(&hawkwindGetIntro);
    dlg->setIntro(intro);
    dlg->setLongIntro(intro);
    dlg->setDefaultAnswer(new Response(string("\n" + hawkwindText[HW_DEFAULT])));

    for (int v = 0; v < VIRT_MAX; v++) {
        string virtue(getVirtueName((Virtue) v));
        lowercase(virtue);
        virtue = virtue.substr(0, 4);
        dlg->addKeyword(virtue, new DynamicResponse(&hawkwindGetAdvice, virtue));
    }

    Response *bye = new Response(hawkwindText[HW_BYE]);
    bye->add(ResponsePart::STOPMUSIC);
    bye->add(ResponsePart::END);
    dlg->addKeyword("bye", bye);
    dlg->addKeyword("", bye);

    return dlg;
}

/**
 * Generate the appropriate response when the player asks Lord British
 * for help.  The help text depends on the current party status; when
 * one quest item is complete, Lord British provides some direction to
 * the next one.
 */
Response *hawkwindGetAdvice(const DynamicResponse *dynResp) {
    string text;
    int virtue = -1, virtueLevel = -1;

    /* check if asking about a virtue */
    for (int v = 0; v < VIRT_MAX; v++) {
        if (strncasecmp(dynResp->getParam().c_str(), getVirtueName((Virtue) v), 4) == 0) {
            virtue = v;
            virtueLevel = c->saveGame->karma[v];
            break;
        }
    }
    if (virtue != -1) {
        text = "\n\n";
        if (virtueLevel == 0)
            text += hawkwindText[HW_ALREADYAVATAR] + "\n";
        else if (virtueLevel < 80)
            text += hawkwindText[(virtueLevel/20) * 8 + virtue];
        else if (virtueLevel < 99)
            text += hawkwindText[3 * 8 + virtue];
        else /* virtueLevel >= 99 */
            text = hawkwindText[4 * 8 + virtue] + hawkwindText[HW_GOTOSHRINE];
    }
    else {
        text = string("\n") + hawkwindText[HW_DEFAULT];
    }

    return new Response(text);
}

Response *hawkwindGetIntro(const DynamicResponse *dynResp) {
    Response *intro = new Response("");

    if (c->party->member(0)->getStatus() == STAT_SLEEPING ||
        c->party->member(0)->getStatus() == STAT_DEAD) {
        intro->add(hawkwindText[HW_SPEAKONLYWITH] + c->party->member(0)->getName() +
                   hawkwindText[HW_RETURNWHEN] + c->party->member(0)->getName() +
                   hawkwindText[HW_ISREVIVED]);
        intro->add(ResponsePart::END);
    }

    else {
        intro->add(ResponsePart::STARTMUSIC_HW);
        intro->add(ResponsePart::HAWKWIND);

        intro->add(hawkwindText[HW_WELCOME] + c->party->member(0)->getName() +
                   hawkwindText[HW_GREETING1] + hawkwindText[HW_GREETING2]);
    }

    return intro;
}
