#include <SPI.h> 
#include <Ucglib.h>

const int FQ_UD = 11; 
const int SDAT = 10; 
const int SCLK = 12; 
const int RESET = 9; 

double prevs[320] = {10};

double _lowestSwrFrq = 0; 
double _lowestSwr = 0; 

double _frqStart = 1; 
double _frqStop = 30; 

double _currFrq; 

int _labelsVShift = 11;

Ucglib_ILI9341_18x240x320_SWSPI ucg(8, 7, 6, 5, 4); // sclk,data,cd,cs,reset 

int btnAflag = 0;
int btnBflag = 0;

int _gridXMin = 16;  
int _gridXMax = 320;  

int _gridYMin = 15;
int _gridYMax = 224;

int _gridWidth = _gridXMax - _gridXMin;
int _gridHeight = _gridYMax - _gridYMin;

double _raster = 3;
int _steps = _gridWidth / _raster; 

double _frqLow = _frqStart; 
double _frqMid = (_frqStart + _frqStop) / 2; 
double _frqHigh = _frqStop;

void setup() {
    ucg.begin(UCG_FONT_MODE_SOLID);

    ucg.setRotate90();
    ucg.setFont(ucg_font_7x14_mf);

    // buttons
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    digitalWrite(2, HIGH);
    digitalWrite(3, HIGH);
    attachInterrupt(0, INT_buttonA, FALLING);
    attachInterrupt(1, INT_buttonB, FALLING);

    pinMode(13, OUTPUT);

    // analog inputs
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    analogReference(INTERNAL); 

    // DDS
    pinMode(FQ_UD, OUTPUT);
    pinMode(SCLK, OUTPUT);
    pinMode(SDAT, OUTPUT);
    pinMode(RESET, OUTPUT);
    
    DDS_init();

    prepareDisplay();
    setFullBandScan();
}

void loop() {
    if (btnAflag == 1) {
        btnAflag = 0;
        btnBflag = 0;
        
        setFullBandScan();
        printScale();
    } 
    
    if (btnBflag == 1) {
        btnAflag = 0;
        btnBflag = 0;
        
        setMinSwrBandScan();
        printScale();
    } 

    PerformScan();
}

void INT_buttonA() {
    btnAflag = 1;
}

void INT_buttonB() {
    btnBflag = 1;
}

void setFullBandScan() {
    ucg.setColor(0, 255, 0);
    ucg.setPrintPos(255, _labelsVShift); ucg.print("Full-band");
    
    _frqStart = 1;
    _frqStop = 30; 
    
    _frqLow = _frqStart; 
    _frqMid = (_frqStop-_frqStart) / 2; 
    _frqHigh = _frqStop;

    _lowestSwrFrq = 0; 
    _lowestSwr = 0;
}

void setMinSwrBandScan() {
    if (_lowestSwrFrq < 2 || _lowestSwrFrq > 29) {
        _lowestSwrFrq = 15;
    }
    
    ucg.setColor(255, 0, 0);
    ucg.setPrintPos(255, _labelsVShift); ucg.print("  Min-SWR");
        
    _frqStart = _lowestSwrFrq - 1.25; 
    _frqStop = _lowestSwrFrq + 1.25; 

    _frqLow = _frqStart; 
    _frqMid = _lowestSwrFrq; 
    _frqHigh = (_frqStop + _frqStart) / 2; 

    _lowestSwrFrq = 0;
    _lowestSwr = 0;
}

void PerformScan() {
    double frqStep = (_frqStop - _frqStart) / _steps;
    double prev = 10;
    double prev2 = 10;
    double fix = 10;
    double swr;
    
    drawGrid();
    
    _lowestSwr = 10;
    
    int X = _gridXMin + 1;
    int Y = 1;
    int Y2 = 1;
    
    for (int i = 1; i < _steps; i++) {
        _currFrq = _frqStart + frqStep * i;

        swr = checkSWR(_currFrq);
        
        if (swr < _lowestSwr && swr > 1) {
            _lowestSwr = swr;
            _lowestSwrFrq = _currFrq;
        }

        // zamazanie starego wykresu
        Y = scaleY(prev2);
        Y2 = scaleY(prevs[i]);
        ucg.setColor(0, 0, 0);
        drawLine(X, Y, X + _raster, Y2);
        prev2 = prevs[i];
		
        // rysowanie nowego wykresu
        Y = scaleY(prev);
        Y2 = scaleY(swr);
        ucg.setColor(255, 255, 0);
        drawLine(X, Y, X + _raster, Y2);
        prev = fix = swr;

        X += _raster;
        prevs[i] = swr;
    }

    refreshValues();
}

int scaleY(double swr){
  int Y = _gridYMax - (swr-1) * _gridHeight / 9;
  if (Y <= _gridYMin) Y = _gridYMin + 1;
  if (Y >= _gridYMax - 3) Y = _gridYMax - 3;
  return Y;
}

void drawLine(int x1, int y1, int x2, int y2) {
    ucg.drawLine(x1, y1, x2, y2);
}

void prepareDisplay() {
    ucg.clearScreen();
    drawGrid();
    printLabels();
    printScale();
    refreshValues();
}

void drawGrid() {
    ucg.setColor(255, 255, 255);
    
    // ramka
    ucg.drawRFrame(_gridXMin, _gridYMin, _gridWidth, _gridHeight, 1);
    
    ucg.setColor(0, 200, 0);
    // poziome
    int y2 = _gridYMax - _gridHeight / 9 * 1;
    int y3 = _gridYMax - _gridHeight / 9 * 2;
    int y5 = _gridYMax - _gridHeight / 9 * 4;
    int y8 = _gridYMax - _gridHeight / 9 * 7;

    ucg.drawHLine(_gridXMin, y2, _gridWidth); //swr 2
    ucg.drawHLine(_gridXMin, y3, _gridWidth); //swr 3
    ucg.drawHLine(_gridXMin, y5, _gridWidth); //swr 5
    ucg.drawHLine(_gridXMin, y8, _gridWidth); //swr 8

    // pionowe
    int xSpace = _gridWidth/4;
    int x1 = _gridXMin + xSpace;
    int x2 = _gridXMin + xSpace * 2;
    int x3 = _gridXMin + xSpace * 3;
    
    ucg.drawVLine(x1, _gridYMin, _gridHeight);
    ucg.drawVLine(x2, _gridYMin, _gridHeight);
    ucg.drawVLine(x3, _gridYMin, _gridHeight);
}

void printLabels() {
    // nad wykresem
    ucg.setColor(255, 255, 255);
    ucg.setPrintPos(18, _labelsVShift); ucg.print("MIN-SWR 1:");
    ucg.setPrintPos(123, _labelsVShift); ucg.print("@");
    ucg.setPrintPos(165, _labelsVShift); ucg.print("MHz");

    // z lewej strony
    int y2 = (_gridYMax - _gridHeight / 9 * 1)+4;
    int y3 = (_gridYMax - _gridHeight / 9 * 2)+4;
    int y5 = (_gridYMax - _gridHeight / 9 * 4)+4;
    int y8 = (_gridYMax - _gridHeight / 9 * 7)+4;
    
    ucg.setPrintPos(_gridXMin - 16, _gridYMin + 12); ucg.print("10");
    ucg.setPrintPos(_gridXMin - 10, y8); ucg.print("8");
    ucg.setPrintPos(_gridXMin - 10, y5); ucg.print("5");
    ucg.setPrintPos(_gridXMin - 10, y3); ucg.print("3");
    ucg.setPrintPos(_gridXMin - 10, y2); ucg.print("2");
    ucg.setPrintPos(_gridXMin - 10, _gridYMax-1 ); ucg.print("1");
}

void refreshValues() {
    // nad wykresem
    ucg.setColor(255, 255, 0);

    ucg.setPrintPos(90, _labelsVShift);
    ucg.print(_lowestSwr, (_lowestSwr < 10 ? 2 : 1));

    ucg.setColor(255, 255, 0);
    ucg.setPrintPos(133, _labelsVShift);
    ucg.print(_lowestSwrFrq, (_lowestSwrFrq < 10 ? 2 : 1));
}

void printScale() {
    // pod wykresem
    ucg.setColor(255, 255, 255);
    ucg.setColor(255, 255, 255);
    int vSpace = 14;

    ucg.setPrintPos(_gridXMin, _gridYMax + vSpace);
    ucg.print(_frqLow, _frqLow < 10 ? 2 : 1);

    ucg.setPrintPos((_gridXMax+_gridXMin)/2-13,  _gridYMax + vSpace);
    ucg.print(_frqMid, _frqMid < 10 ? 2 : 1);

    ucg.setPrintPos(_gridXMax-30,  _gridYMax + vSpace);
    ucg.print(_frqHigh, (_frqHigh < 10 ? 2 : 1));
}

double checkSWR( double _currFrq) {
    DDS_SetFrq(_currFrq);
 
    delay(5);
    
    double FWD = 0;
    double REF = 0;
    double SWR = 10;
    
    REF = analogRead(A0) - 2;
    FWD = analogRead(A1);
    
    REF = REF >= FWD ? FWD - 1 : REF;
    REF = REF < 1 ? 1 : REF;

    SWR = (FWD + REF) / (FWD - REF);
    SWR = SWRcalibrator(SWR);

    return SWR > 10 ? 10 : SWR;
}

double SWRcalibrator(double swr) {
    
    // kalibracja wskazań
    double corr1 = 1.8; // kalibruj na 2 przy 100 omach
    double corr2 = -0.43; // kalibruj na 3 przy 150 omach
    double corr3 = -0.12;  // kalibruj na 4 przy 200 omach
    double corr4 = 0.25; // kalibruj na 5 przy 250 omach
    double corr8 = -0.55; // kalibruj na 8 przy 450 omach 
    
    swr += (swr - 1) > 0  ?  (swr - 1) * corr1  :  0; // > 2
    swr += (swr - 2) > 0  ?  (swr - 2) * corr2  :  0; // > 3
    swr += (swr - 3) > 0  ?  (swr - 3) * corr3  :  0; // > 4
    swr += (swr - 4) > 0  ?  (swr - 4) * corr4  :  0; // > 5
    swr += (swr - 7) > 0  ?  (swr - 7) * corr8  :  0; // > 8
    
    // korekta przechyłu w funkcji częstotliwości i swr
    double frqCorrFactor = 1.5; // siła korekty przechyłu wykresu w funkcji częstotliwości
    double swrCorrFactor = 7; // siła korekty przechyłu wykresu w funkcji swr 

    double frqCorr = _currFrq * frqCorrFactor / _frqStop  * (swr/swrCorrFactor); 
    
    return swr + frqCorr;
}

void DDS_init() {
    digitalWrite(RESET, HIGH); // DDS Reset Sequence
    delay(1);                            
    digitalWrite(RESET, LOW);
    digitalWrite(SCLK, HIGH);         
    digitalWrite(SCLK, LOW);
    digitalWrite(FQ_UD, HIGH);
    digitalWrite(FQ_UD, LOW);
}

void DDS_SetFrq(double Freq_Hz) {
    int32_t f = Freq_Hz * 4294967295 / 125000000  * 1000000;
    for (int b = 0; b < 4; b++, f >>= 8) { 
        DDS_sendByte(f & 0xFF);
    }
    DDS_sendByte(0);                
    digitalWrite(FQ_UD, HIGH);
    digitalWrite(FQ_UD, LOW);
}

void DDS_sendByte(byte data_to_send) {
    for (int i = 0; i < 8; i++, data_to_send >>= 1) { 
        digitalWrite(SDAT, data_to_send & 0x01);
        digitalWrite(SCLK, HIGH);
        digitalWrite(SCLK, LOW);
    }
}
