#include "EDDisplayManager.h"
#include <WebSerial.h>
#include "CrcParameters.h"
#include "CRC8.h"

uint8_t skullSymbol[8] = {
  0b00000,
  0b01110,
  0b10101,
  0b10101,
  0b01110,
  0b01110,
  0b00000,
  0b00000
};

EDDisplayManager::EDDisplayManager(I2CDeviceDisplay *display)
{
    _display = display;
    _display->CreateChar(1, skullSymbol);
    StatusPage = new DisplayPage();
    LocationPage = new DisplayPage();
    CommanderPage = new DisplayPage();
    NavPage = new DisplayPage();
    NavRoutePage = new DisplayPage();
    ErrorPage = new DisplayPage();
    AlertPage = new DisplayPage();
    _current = nullptr;

    _Carousel.push_back(CommanderPage);
    _Carousel.push_back(StatusPage);
    _Carousel.push_back(LocationPage);
    _Carousel.push_back(NavRoutePage);
    _lock = xSemaphoreCreateBinary();
    Unlock();
}

void EDDisplayManager::Display(DisplayPage* page)
{
    if (_current != page) {
        //Lock();
        _buffer.Assign(page);
        //Unlock();
        _display->clear();        
        for(int i=0;i<4;i++)
            _display->PrintAt(_buffer.lines[i], i);
        _current = page;
    }
}

void EDDisplayManager::BlinkLines(bool showBlinkLines)
{
    for(int i=0;i<4;i++)
        if (_buffer.lines[i][0] == '~')
            if (showBlinkLines)
                _display->PrintAt(_buffer.lines[i], i);
            else
                _display->PrintAt(_buffer.BlankLine, i);
}

void EDDisplayManager::Lock()
{
    xSemaphoreTake(_lock, portMAX_DELAY);
}

void EDDisplayManager::Unlock()
{
    xSemaphoreGive(_lock);
}

void EDDisplayManager::UpdatePages()
{
    Lock();
    uint8_t currentCrcResult;
    uint8_t pageCrcResult;
    CRC8 pageCrc;

    pageCrc.reset();
    pageCrc.setInitial(CRC8_INITIAL);
    if (_current != nullptr) {
        pageCrc.add((uint8_t *)_current->lines[0], strlen(_current->lines[0]));
        pageCrc.add((uint8_t *)_current->lines[1], strlen(_current->lines[1]));
        pageCrc.add((uint8_t *)_current->lines[2], strlen(_current->lines[2]));
        pageCrc.add((uint8_t *)_current->lines[3], strlen(_current->lines[3]));
    }
    currentCrcResult = pageCrc.calc();

    if (EDGameVariables.IsShutdown)
    {
        CommanderPage->clearBuffer();
        StatusPage->clearBuffer();
        LocationPage->clearBuffer();
        NavRoutePage->clearBuffer();
        AlertPage->clearBuffer();
        strcpy(StatusPage->lines[0], "~Standby");
    } else {
        if (EDGameVariables.InfosCommanderName != nullptr)
        {
            CommanderPage->clearBuffer();
            strcpy(CommanderPage->lines[0], "$Welcome, Commander");
            snprintf(CommanderPage->lines[1], 21, "$%s", EDGameVariables.InfosCommanderName);
            if (EDGameVariables.IsInMainShip()  && (EDGameVariables.InfosShipName != nullptr) && (strlen(EDGameVariables.InfosShipName) > 0)) {
                snprintf(CommanderPage->lines[2], 21, "$This is %s", EDGameVariables.InfosShipName);
                if (EDGameVariables.IsLanded() || EDGameVariables.IsDocked())
                    strcpy(CommanderPage->lines[3], "$Ready to proceed.");
            }
        }

        if ((EDGameVariables.AlertMessageTitle != nullptr) && (strlen(EDGameVariables.AlertMessageTitle) > 0))
        {
            AlertPage->clearBuffer();
            if (EDGameVariables.AlertDuration > 0) {
                snprintf(AlertPage->lines[0], 21, "~%s", EDGameVariables.AlertMessageTitle);
                snprintf(AlertPage->lines[1], 21, "%s", EDGameVariables.AlertMessage1);
                snprintf(AlertPage->lines[2], 21, "%s", EDGameVariables.AlertMessage2);
                snprintf(AlertPage->lines[3], 21, "%s", EDGameVariables.AlertMessage3);
            }
            alertEnd = millis() + (EDGameVariables.AlertDuration * 1000);
            memset(EDGameVariables.AlertMessageTitle, 0, 21);
            memset(EDGameVariables.AlertMessage1, 0, 21);
            memset(EDGameVariables.AlertMessage2, 0, 21);
            memset(EDGameVariables.AlertMessage3, 0, 21);
        }

        StatusPage->clearBuffer();
        strcpy(StatusPage->lines[0], "$//Status");
        if ((strncmp(EDGameVariables.StatusLegal, "Clean", 5) == 0) || (strncmp(EDGameVariables.StatusLegal, "Speeding", 8) == 0))
            snprintf(StatusPage->lines[1], 21, "$%s", EDGameVariables.StatusLegal);
        else {
            snprintf(StatusPage->lines[1], 21, "~%s", EDGameVariables.StatusLegal);
        }
        if (strlen(EDGameVariables.LocalAllegiance) > 0)
            snprintf(StatusPage->lines[2], 21, "$\xf4 %s", EDGameVariables.LocalAllegiance);
        if (strlen(EDGameVariables.SystemSecurity) > 0)
            snprintf(StatusPage->lines[3], 21, "$\x01 %s", EDGameVariables.SystemSecurity);

        LocationPage->clearBuffer();
        if ((EDGameVariables.LocationStationName != nullptr) && (EDGameVariables.LocationSystemName != nullptr))
        {
            strcpy(LocationPage->lines[0], "$//Location");
            snprintf(LocationPage->lines[1], 21, "$%s", EDGameVariables.LocationSystemName);
            if (strlen(EDGameVariables.LocationStationName) > 0)
                snprintf(LocationPage->lines[2], 21, "$\xf3 %s", EDGameVariables.LocationStationName);
            if (strlen(EDGameVariables.LocalAllegiance) > 0)
                snprintf(LocationPage->lines[3], 21, "$\xf4 %s", EDGameVariables.LocalAllegiance);
        }

        NavRoutePage->clearBuffer();
        if ((EDGameVariables.Navroute1 != nullptr) && (EDGameVariables.Navroute2 != nullptr) && (EDGameVariables.Navroute3 != nullptr))
        {
            if ((EDGameVariables.Navroute1[0] != ' ') && (EDGameVariables.Navroute1[0] != 0)) {
                strcpy(NavRoutePage->lines[0], "$//Nav route");
                snprintf(NavRoutePage->lines[1], 21, "$>%s", EDGameVariables.Navroute1);
                snprintf(NavRoutePage->lines[2], 21, "$ %s", EDGameVariables.Navroute2);
                snprintf(NavRoutePage->lines[3], 21, "$ %s", EDGameVariables.Navroute3);
            }
        }
    }

    // If data in _current page has been updated, invalidate display
    pageCrc.reset();
    pageCrc.setInitial(CRC8_INITIAL);
    if (_current != nullptr) {
        pageCrc.add((uint8_t *)_current->lines[0], strlen(_current->lines[0]));
        pageCrc.add((uint8_t *)_current->lines[1], strlen(_current->lines[1]));
        pageCrc.add((uint8_t *)_current->lines[2], strlen(_current->lines[2]));
        pageCrc.add((uint8_t *)_current->lines[3], strlen(_current->lines[3]));
    }
    pageCrcResult = pageCrc.calc();

    if (currentCrcResult != pageCrcResult)
    {
//        WebSerial.printf("display page content changed, force invalidate %d != %d\n", currentCrcResult, pageCrcResult);
        Invalidate();
    }

    Unlock();
}

void EDDisplayManager::Invalidate()
{
    _current = nullptr;
}

void EDDisplayManager::Handle()
{
    Lock();
    unsigned long now = millis();
    DisplayPage* page = _current;

    if (!AlertPage->IsEmpty())
    {
        if (now > alertEnd)
            AlertPage->clearBuffer();
    }
    
    if (!ErrorPage->IsEmpty())
    {
        page = ErrorPage;
    } 
    else if (!AlertPage->IsEmpty())
    {
        page = AlertPage;
    }
    // If navigation infos available, display it
    else if (!NavPage->IsEmpty())
    {
        page = NavPage;
    }
    else if (now - lastTick > CAROUSEL_PAGE_DURATION) {
        int currentId = _pageIdx; 
        do {
            _pageIdx++;
            if (_pageIdx >= _Carousel.size())
                _pageIdx = 0;
            page = _Carousel.at(_pageIdx);
        } while(page->IsEmpty() && (_pageIdx != currentId));
        lastTick = now;
    }
    if ((page == nullptr) || page->IsEmpty())
    {
        Unlock();
        return;
    } 
    
    if (page != _current) {
//        WebSerial.printf("%lu Displaypage\n", now);
        Display(page);
        // reset timers
        lastBlink = now;
    }
    else {
        bool displayBlinkLines = true;
        uint32_t blinkInterval = BLINK_INTERVAL;
        if (EDGameVariables.IsShutdown) {
            blinkInterval *= 3;            
        }
        if (now - lastBlink > blinkInterval)
        {
            bool displayBlinkLines = (now % (blinkInterval * 2)) < blinkInterval;
            bool hasBlinkLines = (page->lines[0][0] == '~')
                || (page->lines[1][0] == '~')
                || (page->lines[2][0] == '~')
                || (page->lines[3][0] == '~');
            if (hasBlinkLines)
            {
                BlinkLines(displayBlinkLines);
            }
            lastBlink = now;
        } 
    } 
    Unlock(); 
}
