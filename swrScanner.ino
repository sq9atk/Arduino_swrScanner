#include <SPI.h>
#include <Ucglib.h>

const int FQ_UD = 11;
const int SDAT = 10;
const int SCLK = 12;
const int RESET = 9;

const double DDS_FRQ_MIN = 0.5; // MHz
const double DDS_FRQ_MAX = 32; // MHz

double _swrArr[160] = {10};

double _lowestSwrFrq = 0;
double _lowestSwr = 0;

double _frqStart = DDS_FRQ_MIN;
double _frqMid = (DDS_FRQ_MIN + DDS_FRQ_MAX) / 2;
double _frqStop = DDS_FRQ_MAX;

int _scanShift = 0;
const double SCAN_SPAN = 1.5;// MHZ w górę i w dół od czestotliwosci srodkowej

double _currFrq;
double _frqStep;

int _labelsVShift = 10;

Ucglib_ILI9341_18x240x320_SWSPI LCD(8, 7, 6, 5, 4); // sclk,data,cd,cs,reset


int btnAflag = 0;
int btnBflag = 0;

// wymiary i położenie wykresu
const int GRID_X_MIN = 22;
const int GRID_X_MAX = 298;

const int GRID_Y_MIN = 27;
const int GRID_Y_MAX = 225;

const int GRID_WIDTH = GRID_X_MAX - GRID_X_MIN;
const int GRID_HEIGHT = GRID_Y_MAX - GRID_Y_MIN;


const int X_RASTER = 3; // im większa wartość tym gęsciej mierzony SWR ale wolniejszy przebieg
const int STEPS = GRID_WIDTH / X_RASTER;

// wartości Y dla różnych poziomów SWR
const int Y_SWR2 = GRID_Y_MAX - GRID_HEIGHT / 9 * 1; //swr 2
const int Y_SWR3 = GRID_Y_MAX - GRID_HEIGHT / 9 * 2; //swr 3
const int Y_SWR5 = GRID_Y_MAX - GRID_HEIGHT / 9 * 4; //swr 5
const int Y_SWR8 = GRID_Y_MAX - GRID_HEIGHT / 9 * 7; //swr 8

// 3 punkty w środku wykresu
const int X_MID_SPACE = GRID_WIDTH/4;
const int X1_MID = GRID_X_MIN + X_MID_SPACE;
const int X2_MID = GRID_X_MIN + X_MID_SPACE * 2;
const int X3_MID = GRID_X_MIN + X_MID_SPACE * 3;

void setup() {
    LCD.begin(UCG_FONT_MODE_SOLID);

    LCD.setRotate90();
    LCD.setFont(ucg_font_6x13_mf);

    LCD.clearScreen();
    LCD.setColor(255,255,0);
    LCD.setPrintPos(110, 118);
    LCD.print("Arduino SWR Scanner");
    
    LCD.setPrintPos(132, 132);
    LCD.print("SQ9ATK 2019");

    delay(1500);


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

int _scanMode = 1;
void loop() {
    if (btnAflag == 1) {
        if (_scanMode == 1) { }
        if (_scanMode == 2) { }
        if (_scanMode == 3) {
           _scanShift += 2 * SCAN_SPAN;   
        }
        btnAflag = 0;
    }
    
    if (btnBflag == 1) {
        _scanMode++;
        if (_scanMode > 3) {
          _scanMode = 1;
        }
        btnBflag = 0;
    }
 
    if (_scanMode == 1) { // pełne pasmo
        setFullBandScan();
        printYScaleLabels(); 
        _scanShift = 0;        
    }
    if (_scanMode == 2) { // min swr
        setMinSwrScan(); 
        printYScaleLabels();
    }
    if (_scanMode == 3) {// wycinkami
        setPartBandScan(); 
        printYScaleLabels();
    }

    PerformScan();
}

void setFullBandScan() {
    LCD.setColor(255,255,0);
    LCD.setPrintPos(262, _labelsVShift);
    LCD.print("FULL-BAND");

    _frqStart = DDS_FRQ_MIN;
    _frqStop = DDS_FRQ_MAX;
    _frqMid = (DDS_FRQ_MIN + DDS_FRQ_MAX) / 2;

    _frqStep = (_frqStop - _frqStart) / STEPS;
}

void setMinSwrScan() {
    LCD.setColor(255,255,0);
    LCD.setPrintPos(262, _labelsVShift);
    LCD.print("MIN-SWR  ");
    
    _frqMid = _lowestSwrFrq;
    
    if (_frqMid - SCAN_SPAN <  DDS_FRQ_MIN) {
        _frqMid = DDS_FRQ_MIN + SCAN_SPAN;
    }

    if (_frqMid + SCAN_SPAN >  DDS_FRQ_MAX) {
        _frqMid = DDS_FRQ_MAX - SCAN_SPAN;
    }

    _frqStart = _frqMid - SCAN_SPAN;
    _frqStop = _frqMid + SCAN_SPAN;
    _frqStep = (_frqStop - _frqStart) / STEPS;
}

void setPartBandScan() {
    LCD.setColor(255,255,0);
    LCD.setPrintPos(262, _labelsVShift);
    LCD.print("PART-BAND");
    
    _frqMid = DDS_FRQ_MIN + SCAN_SPAN;
    _frqMid += _scanShift;
    
    if (_frqMid - SCAN_SPAN <  DDS_FRQ_MIN) {
        _frqMid = DDS_FRQ_MIN + SCAN_SPAN;
    }

    if (_frqMid + SCAN_SPAN >  DDS_FRQ_MAX) {
        _frqMid = DDS_FRQ_MAX - SCAN_SPAN;
    }

    _frqStart = _frqMid - SCAN_SPAN;
    _frqStop = _frqMid + SCAN_SPAN;
    _frqStep = (_frqStop - _frqStart) / STEPS;
}

void INT_buttonA() {
    btnAflag = 1;
}

void INT_buttonB() {
    btnBflag = 1;
}

void PerformScan() {
    double prev = 10;
    double prev2 = 10;
    double fix = 10;
    double swr;

    _lowestSwr = 10;

    int X = GRID_X_MIN + 1;
    int Y = 1;
    int Y2 = 1;

    refreshValuesPreScan();

    for (int i = 1; i < STEPS; i++) {
        _currFrq = _frqStart + _frqStep * i;

        swr = checkSWR(_currFrq);
  
        if (_scanMode == 1 || _scanMode == 3) {
            if (swr < _lowestSwr && swr > 1) {
              _lowestSwr = swr;
              _lowestSwrFrq = _currFrq;
            }
        }

        int endDrawing = 0;
        int X2 = X + X_RASTER;

        if (X2 >= (GRID_X_MAX-1)) {
            X2 = GRID_X_MAX-2;
            endDrawing = 1;
        }

        // zamazanie starego wykresu
        Y = scaleY(prev2);
        Y2 = scaleY(_swrArr[i]);
        LCD.setColor(0, 0, 0);
        LCD.drawLine(X, Y, X2, Y2);
        repairGridLines(X, Y, X2, Y2);
        prev2 = _swrArr[i];

        // rysowanie nowego wykresu
        Y = scaleY(prev);
        Y2 = scaleY(swr);
        repairGridLines(X, Y, X2, Y2);
        LCD.setColor(255, 255, 0);
        LCD.drawLine(X, Y, X2, Y2);
        prev = fix = swr;

        if (endDrawing == 1) break;

        X += X_RASTER;

        _swrArr[i] = swr;
    }

    refreshValuesPostScan();

    markRezonanses(_frqStep);
}

int scaleY(double swr){
  int Y = GRID_Y_MAX - (swr-1) * GRID_HEIGHT / 9;
  if (Y <= GRID_Y_MIN) Y = GRID_Y_MIN + 1; // ograniczenie wykresu od góry
  if (Y >= GRID_Y_MAX - 3) Y = GRID_Y_MAX - 3; // ograniczenie wykresu od dołu
  return Y;
}

void repairGridLines (int x1, int y1, int x2, int y2) {
    LCD.setColor(0, 200, 0);

    // poziome
    if (y1 >= Y_SWR2 && y2 <= Y_SWR2) LCD.drawHLine(x1, Y_SWR2, x2 - x1 + 1);
    if (y1 >= Y_SWR3 && y2 <= Y_SWR3) LCD.drawHLine(x1, Y_SWR3, x2 - x1 + 1);
    if (y1 >= Y_SWR5 && y2 <= Y_SWR5) LCD.drawHLine(x1, Y_SWR5, x2 - x1 + 1);
    if (y1 >= Y_SWR8 && y2 <= Y_SWR8) LCD.drawHLine(x1, Y_SWR8, x2 - x1 + 1);

    if (y1 <= Y_SWR2 && y2 >= Y_SWR2) LCD.drawHLine(x1, Y_SWR2, x2 - x1 + 1);
    if (y1 <= Y_SWR3 && y2 >= Y_SWR3) LCD.drawHLine(x1, Y_SWR3, x2 - x1 + 1);
    if (y1 <= Y_SWR5 && y2 >= Y_SWR5) LCD.drawHLine(x1, Y_SWR5, x2 - x1 + 1);
    if (y1 <= Y_SWR8 && y2 >= Y_SWR8) LCD.drawHLine(x1, Y_SWR8, x2 - x1 + 1);

    // pionowe
    if (x1 <= X1_MID && x2 >= X1_MID) LCD.drawLine(X1_MID, y1, X1_MID, y2);
    if (x1 <= X2_MID && x2 >= X2_MID) LCD.drawLine(X2_MID, y1, X2_MID, y2);
    if (x1 <= X3_MID && x2 >= X3_MID) LCD.drawLine(X3_MID, y1, X3_MID, y2);

    if (x1 >= X1_MID && x2 <= X1_MID) LCD.drawLine(X1_MID, y1, X1_MID, y2);
    if (x1 >= X2_MID && x2 <= X2_MID) LCD.drawLine(X2_MID, y1, X2_MID, y2);
    if (x1 >= X3_MID && x2 <= X3_MID) LCD.drawLine(X3_MID, y1, X3_MID, y2);
}

void prepareDisplay() {
    LCD.clearScreen();
    drawGrid();
    printLabels();
    printYScaleLabels();
    refreshValuesPostScan();
}

void drawGrid() {
    // poziome
    LCD.setColor(0, 200, 0);
    LCD.drawHLine(GRID_X_MIN + 1, Y_SWR2, GRID_WIDTH - 2); //swr 2
    LCD.drawHLine(GRID_X_MIN + 1, Y_SWR3, GRID_WIDTH - 2); //swr 3
    LCD.drawHLine(GRID_X_MIN + 1, Y_SWR5, GRID_WIDTH - 2); //swr 5
    LCD.drawHLine(GRID_X_MIN + 1, Y_SWR8, GRID_WIDTH - 2); //swr 8

    // pionowe
    LCD.drawVLine(X1_MID, GRID_Y_MIN + 1, GRID_HEIGHT - 2);
    LCD.drawVLine(X2_MID, GRID_Y_MIN + 1, GRID_HEIGHT - 2);
    LCD.drawVLine(X3_MID, GRID_Y_MIN + 1, GRID_HEIGHT - 2);
    // ramka
    LCD.setColor(255,255,255);
    LCD.drawRFrame(GRID_X_MIN, GRID_Y_MIN, GRID_WIDTH, GRID_HEIGHT, 1);
}

void printLabels() {
    // nad wykresem
    LCD.setColor(255, 255, 255);
    LCD.setPrintPos(2, _labelsVShift); LCD.print("MIN-SWR 1:");
    LCD.setPrintPos(100, _labelsVShift); LCD.print("@");
    LCD.setPrintPos(135, _labelsVShift); LCD.print("MHz");
    LCD.setPrintPos(162, _labelsVShift); LCD.print("Step:");
    LCD.setPrintPos(232, _labelsVShift); LCD.print("kHz");

    // z lewej strony
    LCD.setPrintPos(GRID_X_MIN - 21, GRID_Y_MIN + 6); LCD.print("SWR");
    LCD.setPrintPos(GRID_X_MIN - 9, Y_SWR8 + 4); LCD.print("8");
    LCD.setPrintPos(GRID_X_MIN - 9, Y_SWR5 + 4); LCD.print("5");
    LCD.setPrintPos(GRID_X_MIN - 9, Y_SWR3 + 4); LCD.print("3");
    LCD.setPrintPos(GRID_X_MIN - 9, Y_SWR2 + 4); LCD.print("2");
    LCD.setPrintPos(GRID_X_MIN - 9, GRID_Y_MAX-1 ); LCD.print("1");

    // z prawej strony
    LCD.setPrintPos(GRID_X_MAX + 4, GRID_Y_MIN + 6); LCD.print("Z");
    LCD.setPrintPos(GRID_X_MAX + 4, Y_SWR8 + 4); LCD.print("400");
    LCD.setPrintPos(GRID_X_MAX + 4, Y_SWR5 + 4); LCD.print("250");
    LCD.setPrintPos(GRID_X_MAX + 4, Y_SWR3 + 4); LCD.print("150");
    LCD.setPrintPos(GRID_X_MAX + 4, Y_SWR2 + 4); LCD.print("100");
    LCD.setPrintPos(GRID_X_MAX + 4, GRID_Y_MAX-1 ); LCD.print("50");
}

void refreshValuesPreScan() {
    // nad wykresem
    LCD.setColor(255, 255, 0);
    LCD.setPrintPos(193, _labelsVShift);
    LCD.print(_frqStep * 1000, _frqStep < 100 ? 2 : 1);
}

void refreshValuesPostScan() {
    // nad wykresem
    LCD.setColor(255, 255, 0);

    LCD.setPrintPos(63, _labelsVShift);
    LCD.print(_lowestSwr, (_lowestSwr < 10 ? 2 : 1));

    LCD.setPrintPos(108, _labelsVShift);
    LCD.print(_lowestSwrFrq, (_lowestSwrFrq < 10 ? 2 : 1));
}

void printYScaleLabels() {
    // pod wykresem
    LCD.setColor(255, 255, 255);
    LCD.setColor(255, 255, 255);
    int vSpace = 13;

    int width = GRID_X_MAX - GRID_X_MIN;

    LCD.setPrintPos(GRID_X_MIN-2, GRID_Y_MAX + vSpace);
    LCD.print(_frqStart, _frqStart < 10 ? 2 : 1);

    LCD.setPrintPos(GRID_X_MIN +(width*0.25) - 10,  GRID_Y_MAX + vSpace);
    LCD.print((_frqStart+_frqMid)/2, (_frqStart+_frqMid)/2 < 10 ? 2 : 1);  

    LCD.setPrintPos(GRID_X_MIN +(width*0.5) - 10,  GRID_Y_MAX + vSpace);
    LCD.print(_frqMid, _frqMid < 10 ? 2 : 1);
    
    LCD.setPrintPos(GRID_X_MIN +(width*0.75) - 10,  GRID_Y_MAX + vSpace);
    LCD.print((_frqStop+_frqMid)/2, (_frqStop+_frqMid)/2 < 10 ? 2 : 1);  

    LCD.setPrintPos(GRID_X_MAX-22,  GRID_Y_MAX + vSpace);
    LCD.print(_frqStop, (_frqStop < 10 ? 2 : 1));

    LCD.setPrintPos(GRID_X_MAX + 5, GRID_Y_MAX + vSpace);
    LCD.print("MHz");
}

double checkSWR( double _currFrq) {
    DDS_SetFrq(_currFrq);

    //delay(2); // czasem trzeba dodac opóżnienie pomiędzy ustawieniem czestotliwosci a rozpoczęciem pomiaru
                // tutaj opóżnienie realizowane jako czas rysowania kroków wykresu

    double FWD = 0;
    double REF = 0;
    double SWR = 10;

    REF = analogRead(A0) - 3; // korekcja symetrii układu pomiarowego // dobrać przy 50om
    FWD = analogRead(A1);

    REF = REF >= FWD ? FWD - 1 : REF;
    REF = REF < 1 ? 1 : REF;

    SWR = (FWD + REF) / (FWD - REF);
    SWR = SWRcalibrator(SWR);

    return SWR > 10 ? 10 : SWR;
}

void markRezonanses(double _frqStep){
    // oznaczanie rezonansów/dołków

    LCD.setFont(ucg_font_5x8_mr);
    LCD.setColor(0,0,0);
    LCD.drawBox(0, GRID_Y_MIN-11, 320, 8);

    const double DETECT_LEVEL = 0.2; // jak bardzo ostre rezonanse mają być wykrywane
    const int MAX_DETECT_SWR = 10; // pomijamy
    const int LABELS_SHIFT = -5; // poprostu korekcja przesunięcia labeli
    const double LABELS_SPREAD_FACTOR = 1.15; // troszkę rozsuwamy labele poza wykres
    const int LABEL_MIN_DIST = 12; // minimalna odległość wykrytych rezonansów (zeby się nie nakładały)


    int holeSeekSpan = (1/(_frqStop - _frqStart)/3 * 150) +2; // też pomaga wykrywać zaokrąglone dołki przy powiększeniu wykresu
                                                              // działa proporcjonalnie do szerokości zakresu

    int X = GRID_X_MIN;
    int prevX = 0;

    for (int i = 1; i < STEPS; i++) {

        if (_swrArr[i-holeSeekSpan] > _swrArr[i] && _swrArr[i] < _swrArr[i+holeSeekSpan] ) { // wyszukujemy dołek

            if (_swrArr[i-holeSeekSpan] - _swrArr[i] <= DETECT_LEVEL ) continue; // za mało strome zbocze z lewej / pomijamy
            if (_swrArr[i+holeSeekSpan] - _swrArr[i] <= DETECT_LEVEL) continue; // za mało strome zbocze z prawej / pomijamy
            if (_swrArr[i] >= MAX_DETECT_SWR) continue; // zbyt duży swr w dołku. pomijamy.  / pomijamy

            if (X - prevX <= LABEL_MIN_DIST && prevX != 0) continue; // czy nie za blisko poprzedniego rezonansu

            double fRez = _frqStart + _frqStep * (i+1);

            LCD.setColor(255, 0, 0);
            LCD.setPrintPos(X * LABELS_SPREAD_FACTOR + LABELS_SHIFT, GRID_Y_MIN-4);
            LCD.print(fRez, 1);

            prevX = X;
        }

        X += X_RASTER;
    }
    LCD.setFont(ucg_font_6x13_mf);
}
void dump(double var){
    LCD.setColor(255, 0, 255);
    LCD.setPrintPos(272, 10);
    LCD.print(var,2);
}


double SWRcalibrator(double swr) {
    // kalibracja wskazań
    // kalibrujemy w kolejnosci: 100,150,200,250,400 ohm

    double corr1 = 1.8; // kalibruj na 2 przy 100 omach
    double corr2 = -0.43; // kalibruj na 3 przy 150 omach
    double corr3 = -0.12;  // kalibruj na 4 przy 200 omach
    double corr4 = 0.25; // kalibruj na 5 przy 250 omach
    double corr8 = -0.55; // kalibruj na 8 przy 400 omach

    swr += (swr - 1) > 0  ?  (swr - 1) * corr1  :  0; // dla > 2
    swr += (swr - 2) > 0  ?  (swr - 2) * corr2  :  0; // dla > 3
    swr += (swr - 3) > 0  ?  (swr - 3) * corr3  :  0; // dla > 4
    swr += (swr - 4) > 0  ?  (swr - 4) * corr4  :  0; // dla > 5
    swr += (swr - 7) > 0  ?  (swr - 7) * corr8  :  0; // dla > 8


    double frqCorrFactor = 1.5; // siła korekty przechyłu wykresu w funkcji częstotliwości // doświadczalnie dla róznych SWR-ów i QRG
    double swrCorrFactor = 7; // siła korekty przechyłu wykresu w funkcji swr // doświadczalnie dla róznych SWR-ów i QRG

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
