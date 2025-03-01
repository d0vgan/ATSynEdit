{
Copyright (C) Alexey Torgashin, uvviewsoft.com
License: MPL 2.0 or LGPL
}
unit ATStringProc;

{$mode objfpc}{$H+}
{$codepage utf8}

interface

uses
  Classes, SysUtils, StrUtils,
  LCLType, LCLIntf, Clipbrd,
  ATSynEdit_Globals,
  ATSynEdit_UnicodeData,
  ATSynEdit_RegExpr,
  ATSynEdit_CharSizer;

type
  atString = UnicodeString;
  atChar = WideChar;
  PatChar = PWideChar;

const
  ATEditorCharXScale = 1024;

type
  TATEditorCharSize = record
    //XScaled is the average-char-width (usually of 'N'),
    //multiplied by ATEditorCharXScale and truncated.
    //on Win32/Gtk2, XScaled is multiple of ATEditorCharXScale; but not on Qt5/macOS.
    //macOS has actually floating-number font width, e.g. 7.801 pixels of a single char in monospaced fonts
    //(all ASCII chars have the same width, it was tested on macOS).
    XScaled: Int64;
    //XSpacePercents is the width of space-char in percents (of average-char-width).
    //if FontProportional=False, it is 100; otherwise it's different.
    XSpacePercents: Int64;
    //line height.
    //macOS height is still an integer number.
    Y: Int64;
  end;

type
  TATLineChangeKind = (
    cLineChangeEdited,
    cLineChangeAdded,
    cLineChangeDeleted,
    cLineChangeDeletedAll
    );

type
  TATIntArray = array of integer;
  TATPointArray = array of TPoint;
  TATInt64Array = array of Int64;

type
  TATMarkerMarkerRecord = record
    Tag: Int64;
    TagEx: Int64;
    PosX: integer;
    PosY: integer;
    SelX: integer;
    SelY: integer;
    MicromapMode: integer;
  end;
  TATMarkerMarkerArray = array of TATMarkerMarkerRecord;

type
  TATMarkerAttribRecord = record
    Tag: Int64;
    TagEx: Int64;
    PosX: integer;
    PosY: integer;
    SelX: integer;
    ColorFont: integer;
    ColorBG: integer;
    ColorBorder: integer;
    FontStyles: integer;
    BorderLeft: integer;
    BorderRight: integer;
    BorderUp: integer;
    BorderDown: integer;
    MicromapMode: integer;
  end;
  TATMarkerAttribArray = array of TATMarkerAttribRecord;

const
  //must be >= ATEditorOptions.MaxLineLenForAccurateCharWidths
  cMaxFixedArray = 1024;

type
  //must be with Int64 items, 32-bit is not enough for single line with len>40M
  TATIntFixedArray = record
    Data: packed array[0..cMaxFixedArray-1] of Int64;
    Len: integer;
  end;

  //must be with 'longint' items, it's for Dx offsets for rendering
  TATInt32FixedArray = record
    Data: packed array[0..cMaxFixedArray-1] of Longint;
    Len: integer;
  end;

type
  TATSimpleRange = record NFrom, NTo: integer; end;
  TATSimpleRangeArray = array of TATSimpleRange;

function IsStringWithUnicode(const S: string): boolean; inline;
function IsStringWithUnicode(const S: UnicodeString): boolean; inline;

function SCharUpper(ch: WideChar): WideChar; inline;
function SCharLower(ch: WideChar): WideChar; inline;

function SCaseTitle(const S, SNonWordChars: atString): atString;
function SCaseInvert(const S: atString): atString;
function SCaseSentence(const S, SNonWordChars: atString): atString;

function StringOfCharW(ch: WideChar; Len: integer): UnicodeString;

{$Z1}
type
  TATLineEnds = (cEndNone, cEndWin, cEndUnix, cEndMac);

  TATLineState = (
    cLineStateNone,
    cLineStateChanged,
    cLineStateAdded,
    cLineStateSaved
    );

const
  cLineEndOsDefault = {$ifdef windows} cEndWin {$else} cEndUnix {$endif};
  cLineEndStrings: array[TATLineEnds] of UnicodeString = ('', #13#10, #10, #13);
  cLineEndNiceNames: array[TATLineEnds] of string = ('', 'CRLF', 'LF', 'CR');
  cLineEndLength: array[TATLineEnds] of integer = (0, 2, 1, 1);

const
  BoolToPlusMinusOne: array[boolean] of integer = (-1, 1);

type
  TATStringTabCalcEvent = function(Sender: TObject; ALineIndex, ACharIndex: integer): integer of object;
  TATStringGetLenEvent = function(ALineIndex: integer): integer of object;

type

  { TATStringTabHelper }

  TATStringTabHelper = class
  private
    CharSizer: TATCharSizer;
    //these arrays are local vars, placed here to alloc 2*4Kb not in stack
    ListEnds: TATIntFixedArray;
    ListMid: TATIntFixedArray;
  public
    TabSpaces: boolean;
    TabSize: integer;
    IndentSize: integer;
    CharSize: TATEditorCharSize;
    FontProportional: boolean;
    SenderObj: TObject;
    OnCalcTabSize: TATStringTabCalcEvent;
    OnCalcLineLen: TATStringGetLenEvent;
    constructor Create(ACharSizer: TATCharSizer);
    function CalcTabulationSize(ALineIndex, APos: integer): integer;
    function TabsToSpaces(ALineIndex: integer; const S: atString): atString;
    function TabsToSpaces_Length(ALineIndex: integer; const S: atString; AMaxLen: integer): integer;
    function SpacesToTabs(ALineIndex: integer; const S: atString): atString;
    function GetIndentExpanded(ALineIndex: integer; const S: atString): integer;
    function CharPosToColumnPos(ALineIndex: integer; const S: atString; APos: integer): integer;
    function ColumnPosToCharPos(ALineIndex: integer; const S: atString; AColumn: integer): integer;
    function IndentUnindent(ALineIndex: integer; const Str: atString; ARight: boolean): atString;
    procedure CalcCharOffsets(ALineIndex: integer; const S: atString; out AInfo: TATIntFixedArray; ACharsSkipped: integer = 0);
    function CalcCharOffsetLast(ALineIndex: integer; const S: atString; ACharsSkipped: integer = 0): Int64;
    function FindWordWrapOffset(ALineIndex: integer; const S: atString; AColumns: Int64;
      const ANonWordChars: atString; AWrapIndented: boolean): integer;
    function FindClickedPosition(ALineIndex: integer; const Str: atString;
      constref ListOffsets: TATIntFixedArray;
      APixelsFromLeft: Int64;
      AAllowVirtualPos: boolean;
      out AEndOfLinePos: boolean): Int64;
    procedure FindOutputSkipOffset(ALineIndex: integer;
      const S: atString;
      const AScrollPosSmooth: Int64;
      out ACharsSkipped: Int64;
      out ACellPercentsSkipped: Int64);
  end;

function IsCharEol(ch: widechar): boolean; inline;
function IsCharEol(ch: char): boolean; inline;
function IsCharWord(ch: widechar; const ANonWordChars: UnicodeString): boolean;
function IsCharWordA(ch: char): boolean; inline;
function IsCharWordInIdentifier(ch: widechar): boolean;
function IsCharDigit(ch: widechar): boolean; inline;
function IsCharDigit(ch: char): boolean; inline;
function IsCharSpace(ch: widechar): boolean; inline;
function IsCharSpace(ch: char): boolean; inline;
function IsCharSymbol(ch: widechar): boolean;
function IsCharHexDigit(ch: widechar): boolean; inline;
function IsCharHexDigit(ch: char): boolean; inline;
function HexDigitToInt(ch: char): integer;

function IsCharSurrogateAny(ch: widechar): boolean; inline;
function IsCharSurrogateHigh(ch: widechar): boolean; inline;
function IsCharSurrogateLow(ch: widechar): boolean; inline;

function IsStringSpaces(const S: atString): boolean; inline;
function IsStringSpaces(const S: atString; AFrom, ALen: integer): boolean;

function SBeginsWith(const S, SubStr: UnicodeString): boolean;
function SBeginsWith(const S, SubStr: string): boolean;
function SBeginsWith(const S: UnicodeString; ch: WideChar): boolean; inline;
function SBeginsWith(const S: string; ch: char): boolean; inline;
function SEndsWith(const S, SubStr: UnicodeString): boolean;
function SEndsWith(const S, SubStr: string): boolean;
function SEndsWith(const S: UnicodeString; ch: WideChar): boolean; inline;
function SEndsWith(const S: string; ch: char): boolean; inline;
function SEndsWithEol(const S: string): boolean; inline;
function SEndsWithEol(const S: atString): boolean; inline;

function STrimAll(const S: UnicodeString): UnicodeString;
function STrimLeft(const S: UnicodeString): UnicodeString;
function STrimRight(const S: UnicodeString): UnicodeString;

function SGetIndentChars(const S: string): integer;
function SGetIndentChars(const S: atString): integer;
function SGetIndentCharsToOpeningBracket(const S: atString): integer;
function SGetTrailingSpaceChars(const S: atString): integer;
function SGetNonSpaceLength(const S: atString): integer;

function SStringHasEol(const S: atString): boolean; inline;
function SStringHasEol(const S: string): boolean; inline;
function SStringHasTab(const S: atString): boolean; inline;
function SStringHasTab(const S: string): boolean; inline;
//function SStringHasAsciiAndNoTabs(const S: atString): boolean;
//function SStringHasAsciiAndNoTabs(const S: string): boolean;

function SRemoveNewlineChars(const S: atString): atString;

function SGetItem(var S: string; const ch: Char = ','): string;
procedure SSwapEndianWide(var S: UnicodeString);
procedure SSwapEndianUCS4(var S: UCS4String); inline;
procedure SAddStringToHistory(const S: string; List: TStrings; MaxItems: integer);

procedure TrimStringList(L: TStringList); inline;

procedure SReplaceAll(var S: string; const SFrom, STo: string); inline;
procedure SDeleteFrom(var s: string; const SFrom: string); inline;
procedure SDeleteFrom(var s: UnicodeString; const SFrom: UnicodeString); inline;
procedure SDeleteFromEol(var S: string);
procedure SDeleteFromEol(var S: UnicodeString);

procedure SClipboardCopy(AText: string; AClipboardObj: TClipboard=nil);
function SFindCharCount(const S: string; ch: char): integer;
function SFindCharCount(const S: UnicodeString; ch: WideChar): integer;
function SFindRegexMatch(const Subject, Regex: UnicodeString; out MatchPos, MatchLen: integer): boolean;
function SFindRegexMatch(const Subject, Regex: UnicodeString; GroupIndex: integer; ModS, ModI, ModM: boolean): UnicodeString;
function SCountTextOccurrences(const SubStr, Str: UnicodeString): integer;
function SCountTextLines(const Str, StrBreak: UnicodeString): integer;
procedure SSplitByChar(const S: string; Sep: char; out S1, S2: string);


implementation

uses
  Dialogs, Math;

function IsCharEol(ch: widechar): boolean;
begin
  Result:= (ch=#10) or (ch=#13);
end;

function IsCharEol(ch: char): boolean;
begin
  Result:= (ch=#10) or (ch=#13);
end;

function IsCharWord(ch: widechar; const ANonWordChars: UnicodeString): boolean;
begin
  //to make '_' non-word char, specify it as _first_ in ANonWordChars
  if ch='_' then
    exit( (ANonWordChars='') or (ANonWordChars[1]<>'_') );

  //if it's unicode letter, return true, ignore option string
  //if it's not letter, check for option (maybe char '$' is present there for PHP lexer)
  // bit 7 in value: is word char
  Result := CharCategoryArray[Ord(ch)] and 128 <> 0;
  if not Result then
  begin
    if Ord(ch)<=32 then
      exit(false);

    if IsCharUnicodeSpace(ch) then
      exit(false);

    if Pos(ch, ANonWordChars)=0 then
      exit(true);
  end;
end;

function IsCharWordA(ch: char): boolean;
begin
  // bit 7 in value: is word char
  Result := CharCategoryArray[Ord(ch)] and 128 <> 0;
end;


function IsCharWordInIdentifier(ch: widechar): boolean;
begin
  if Ord(ch)>Ord('z') then
    exit(false);
  case ch of
    '0'..'9',
    'a'..'z',
    'A'..'Z',
    '_':
      Result:= true;
    else
      Result:= false;
  end;
end;

function IsCharDigit(ch: widechar): boolean;
begin
  Result:= (ch>='0') and (ch<='9');
end;

function IsCharDigit(ch: char): boolean;
begin
  Result:= (ch>='0') and (ch<='9');
end;

function IsCharHexDigit(ch: widechar): boolean;
begin
  case ch of
    '0'..'9',
    'a'..'f',
    'A'..'F':
      Result:= true
    else
      Result:= false;
  end;
end;

function IsCharHexDigit(ch: char): boolean;
begin
  case ch of
    '0'..'9',
    'a'..'f',
    'A'..'F':
      Result:= true
    else
      Result:= false;
  end;
end;

function IsCharSpace(ch: widechar): boolean;
begin
  Result:= IsCharUnicodeSpace(ch);
end;

function IsCharSpace(ch: char): boolean;
begin
  case ch of
    #9, //tab
    ' ', //space
    #$A0: //no-break space, NBSP, often used on macOS
      Result:= true;
    else
      Result:= false;
  end;
end;

function IsCharSymbol(ch: widechar): boolean;
begin
  Result:= Pos(ch, '.,;:''"/\-+*=()[]{}<>?!@#$%^&|~`')>0;
end;

function IsCharSurrogateAny(ch: widechar): boolean;
begin
  Result:= (ch>=#$D800) and (ch<=#$DFFF);
end;

function IsCharSurrogateHigh(ch: widechar): boolean;
begin
  Result:= (ch>=#$D800) and (ch<=#$DBFF);
end;

function IsCharSurrogateLow(ch: widechar): boolean;
begin
  Result:= (ch>=#$DC00) and (ch<=#$DFFF);
end;


function IsStringSpaces(const S: atString): boolean;
var
  i: integer;
begin
  for i:= 1 to Length(S) do
    if not IsCharSpace(S[i]) then
      exit(false);
  Result:= true;
end;

function IsStringSpaces(const S: atString; AFrom, ALen: integer): boolean;
var
  i: integer;
begin
  for i:= AFrom to Min(AFrom+ALen-1, Length(S)) do
    if not IsCharSpace(S[i]) then
      exit(false);
  Result:= true;
end;

{
function SStringHasUnicodeChars(const S: atString): boolean;
var
  i, N: integer;
begin
  for i:= 1 to Length(S) do
  begin
    N:= Ord(S[i]);
    if (N<32) or (N>126) then exit(true);
  end;
  Result:= false;
end;
}

procedure DoDebugOffsets(const Info: TATIntFixedArray);
var
  i: integer;
  s: string;
begin
  s:= '';
  for i:= 0 to Info.Len-1 do
    s:= s+IntToStr(Info.Data[i])+'% ';
  ShowMessage('Offsets'#10+s);
end;

{
    Blocks Containing Han Ideographs
    Han ideographic characters are found in five main blocks of the Unicode Standard, as shown in Table 12-2
Table 12-2. Blocks Containing Han Ideographs
Block                                   Range       Comment
CJK Unified Ideographs                  4E00-9FFF   Common
CJK Unified Ideographs Extension A      3400-4DBF   Rare
CJK Unified Ideographs Extension B      20000-2A6DF Rare, historic
CJK Unified Ideographs Extension C      2A700–2B73F Rare, historic
CJK Unified Ideographs Extension D      2B740–2B81F Uncommon, some in current use
CJK Unified Ideographs Extension E      2B820–2CEAF Rare, historic
CJK Compatibility Ideographs            F900-FAFF   Duplicates, unifiable variants, corporate characters
CJK Compatibility Ideographs Supplement 2F800-2FA1F Unifiable variants
}

function _IsCJKText(ch: widechar): boolean; inline;
begin
  case Ord(ch) of
    $4E00..$9FFF,
    $3400..$4DBF,
    $F900..$FAFF:
      Result:= true;
    else
      Result:= false;
  end;
end;

function _IsCJKPunctuation(ch: widechar): boolean; inline;
begin
  case Ord(ch) of
    $3002,
    $ff01,
    $ff0c,
    $ff1a,
    $ff1b,
    $ff1f:
      Result:= true;
    else
      Result:= false;
  end;
end;

function TATStringTabHelper.FindWordWrapOffset(ALineIndex: integer; const S: atString; AColumns: Int64;
  const ANonWordChars: atString; AWrapIndented: boolean): integer;
  //
  //override IsCharWord to check also commas,dots,quotes
  //to wrap them with wordchars
  function _IsWord(ch: widechar): boolean;
  begin
    if _IsCJKText(ch) then
      Result:= false
    else
    if Pos(ch, ATEditorOptions.CommaCharsWrapWithWords)>0 then
      Result:= true
    else
      Result:= IsCharWord(ch, ANonWordChars);
  end;
  //
var
  N, NMin, NAvg: integer;
  Offsets: TATIntFixedArray;
  ch, ch_next: widechar;
begin
  if S='' then
    Exit(0);
  if AColumns<ATEditorOptions.MinWordWrapOffset then
    Exit(AColumns);

  CalcCharOffsets(ALineIndex, S, Offsets);

  if Offsets.Data[Offsets.Len-1]<=AColumns*100 then
    Exit(Length(S));

  //NAvg is average wrap offset, we use it if no correct offset found
  N:= Min(Length(S), cMaxFixedArray)-1;
  while (N>0) and (Offsets.Data[N]>(AColumns+1)*100) do Dec(N);
  NAvg:= N;
  if NAvg<ATEditorOptions.MinWordWrapOffset then
    Exit(ATEditorOptions.MinWordWrapOffset);

  NMin:= SGetIndentChars(S)+1;
  repeat
    ch:= S[N];
    ch_next:= S[N+1];

    if (N>NMin) and
     (IsCharSurrogateLow(ch_next) or //don't wrap inside surrogate pair
      (_IsCJKText(ch) and _IsCJKPunctuation(ch_next)) or //don't wrap between CJK char and CJK punctuation
      (_IsWord(ch) and _IsWord(ch_next)) or //don't wrap between 2 word-chars
      (AWrapIndented and IsCharSpace(ch_next)) //space as 2nd char looks bad with Python sources
     )
    then
      Dec(N)
    else
      Break;
  until false;

  if N>NMin then
    Result:= N
  else
    Result:= NAvg;
end;

function SGetIndentChars(const S: string): integer;
begin
  Result:= 0;
  while (Result<Length(S)) and IsCharSpace(S[Result+1]) do
    Inc(Result);
end;

function SGetIndentChars(const S: atString): integer;
begin
  Result:= 0;
  while (Result<Length(S)) and IsCharSpace(S[Result+1]) do
    Inc(Result);
end;

function SGetTrailingSpaceChars(const S: atString): integer;
var
  N: integer;
begin
  Result:= 0;
  N:= Length(S);
  while (N>0) and IsCharSpace(S[N]) do
  begin
    Inc(Result);
    Dec(N);
  end;
end;


function SGetIndentCharsToOpeningBracket(const S: atString): integer;
var
  n: integer;
begin
  Result:= 0;
  n:= Length(S);
  //note RPos() don't work with UnicodeString
  while (n>0) and (S[n]<>'(') do Dec(n);
  if n>0 then
    //test that found bracket is not closed
    if PosEx(')', S, n)=0 then
      Result:= n;
end;

function SGetNonSpaceLength(const S: atString): integer;
begin
  Result:= Length(S);
  while (Result>0) and IsCharSpace(S[Result]) do Dec(Result);
  if Result=0 then
    Result:= Length(S);
end;

procedure SSwapEndianWide(var S: UnicodeString);
var
  i: integer;
  P: PWord;
begin
  if S='' then exit;
  UniqueString(S);
  for i:= 1 to Length(S) do
  begin
    P:= @S[i];
    P^:= SwapEndian(P^);
  end;
end;

procedure SSwapEndianUCS4(var S: UCS4String);
var
  i: integer;
begin
  for i:= 0 to Length(S)-1 do
    S[i]:= SwapEndian(S[i]);
end;

constructor TATStringTabHelper.Create(ACharSizer: TATCharSizer);
begin
  inherited Create;
  CharSizer:= ACharSizer;
end;

function TATStringTabHelper.CalcTabulationSize(ALineIndex, APos: integer): integer;
begin
  if Assigned(OnCalcTabSize) then
    Result:= OnCalcTabSize(SenderObj, ALineIndex, APos)
  else
  if Assigned(OnCalcLineLen) and (ALineIndex>=0) and (OnCalcLineLen(ALineIndex)>ATEditorOptions.MaxLineLenForAccurateCharWidths) then
    Result:= 1
  else
  if APos>ATEditorOptions.MaxTabPositionToExpand then
    Result:= 1
  else
    Result:= TabSize - (APos-1) mod TabSize;
end;


function TATStringTabHelper.GetIndentExpanded(ALineIndex: integer; const S: atString): integer;
var
  ch: widechar;
  i: integer;
begin
  Result:= 0;
  for i:= 1 to Length(S) do
  begin
    ch:= S[i];
    if not IsCharSpace(ch) then exit;
    if ch<>#9 then
      Inc(Result)
    else
      Inc(Result, CalcTabulationSize(ALineIndex, Result+1));
  end;
end;


function TATStringTabHelper.TabsToSpaces(ALineIndex: integer; const S: atString): atString;
var
  N, NSize: integer;
begin
  Result:= S;
  N:= 0;
  repeat
    N:= PosEx(#9, Result, N+1);
    if N=0 then Break;
    NSize:= CalcTabulationSize(ALineIndex, N);
    if NSize<2 then
      Result[N]:= ' '
    else
    begin
      Result[N]:= ' ';
      Insert(StringOfCharW(' ', NSize-1), Result, N);
    end;
  until false;
end;

function TATStringTabHelper.TabsToSpaces_Length(ALineIndex: integer; const S: atString;
  AMaxLen: integer): integer;
var
  i: integer;
begin
  Result:= 0;
  if AMaxLen<0 then
    AMaxLen:= Length(S);
  for i:= 1 to AMaxLen do
    if S[i]<>#9 then
      Inc(Result)
    else
      Inc(Result, CalcTabulationSize(ALineIndex, Result+1));
end;


procedure TATStringTabHelper.CalcCharOffsets(ALineIndex: integer; const S: atString;
  out AInfo: TATIntFixedArray; ACharsSkipped: integer=0);
var
  NLen, NSize, NTabSize, NCharsSkipped: integer;
  NScalePercents: integer;
  ch: widechar;
  i: integer;
begin
  AInfo:= Default(TATIntFixedArray);
  NLen:= Min(Length(S), cMaxFixedArray);
  AInfo.Len:= NLen;
  if NLen=0 then Exit;

  NCharsSkipped:= ACharsSkipped;

  if NLen>ATEditorOptions.MaxLineLenForAccurateCharWidths then
  begin
    for i:= 0 to NLen-1 do
      AInfo.Data[i]:= (Int64(i)+1)*100;
    exit;
  end;

  for i:= 1 to NLen do
  begin
    ch:= S[i];
    Inc(NCharsSkipped);

    if IsCharSurrogateAny(ch) then
      NScalePercents:= ATEditorOptions.EmojiWidthPercents div 2
    else
      NScalePercents:= CharSizer.GetCharWidth(ch);

    if ch<>#9 then
      NSize:= 1
    else
    if FontProportional then
      NSize:= 1
    else
    begin
      NTabSize:= CalcTabulationSize(ALineIndex, NCharsSkipped);
      NSize:= NTabSize;
      Inc(NCharsSkipped, NTabSize-1);
    end;

    if i=1 then
      AInfo.Data[i-1]:= Int64(NSize)*NScalePercents
    else
      AInfo.Data[i-1]:= AInfo.Data[i-2]+Int64(NSize)*NScalePercents;
  end;
end;

function TATStringTabHelper.CalcCharOffsetLast(ALineIndex: integer; const S: atString;
  ACharsSkipped: integer=0): Int64;
var
  NLen, NSize, NTabSize, NCharsSkipped: integer;
  NScalePercents: integer;
  ch: WideChar;
  i: integer;
begin
  Result:= 0;
  NLen:= Length(S);
  if NLen=0 then Exit;

  if NLen>ATEditorOptions.MaxLineLenForAccurateCharWidths then
    exit(NLen*100);

  NCharsSkipped:= ACharsSkipped;

  for i:= 1 to NLen do
  begin
    ch:= S[i];
    Inc(NCharsSkipped);

    if IsCharSurrogateAny(ch) then
    begin
      NScalePercents:= ATEditorOptions.EmojiWidthPercents div 2;
    end
    else
    begin
      NScalePercents:= CharSizer.GetCharWidth(ch);
    end;

    if ch<>#9 then
      NSize:= 1
    else
    if FontProportional then
      NSize:= 1
    else
    begin
      NTabSize:= CalcTabulationSize(ALineIndex, NCharsSkipped);
      NSize:= NTabSize;
      Inc(NCharsSkipped, NTabSize-1);
    end;

    Inc(Result, Int64(NSize)*NScalePercents);
  end;
end;


function TATStringTabHelper.FindClickedPosition(ALineIndex: integer; const Str: atString;
  constref ListOffsets: TATIntFixedArray;
  APixelsFromLeft: Int64;
  AAllowVirtualPos: boolean;
  out AEndOfLinePos: boolean): Int64;
var
  i: integer;
begin
  AEndOfLinePos:= false;

  if Str='' then
  begin
    Result:= 1;
    if AAllowVirtualPos then
      Inc(Result, Round(APixelsFromLeft * ATEditorCharXScale / CharSize.XScaled));
      //use Round() to fix CudaText issue #4240
    Exit;
  end;

  ListEnds.Len:= ListOffsets.Len;
  ListMid.Len:= ListOffsets.Len;

  //positions of each char end
  for i:= 0 to ListOffsets.Len-1 do
    ListEnds.Data[i]:= ListOffsets.Data[i]*CharSize.XScaled div ATEditorCharXScale div 100;

  //positions of each char middle
  for i:= 0 to ListOffsets.Len-1 do
    if i=0 then
      ListMid.Data[i]:= ListEnds.Data[i] div 2
    else
      ListMid.Data[i]:= (ListEnds.Data[i-1]+ListEnds.Data[i]) div 2;

  for i:= 0 to ListOffsets.Len-1 do
    if APixelsFromLeft<ListMid.Data[i] then
    begin
      Result:= i+1;

      //don't get position inside utf16 surrogate pair
      if (Result<=Length(Str)) and IsCharSurrogateLow(Str[Result]) then
        Inc(Result);

      Exit
    end;

  AEndOfLinePos:= true;

  Result:= ListEnds.Len + Round((APixelsFromLeft - ListEnds.Data[ListEnds.Len-1]) * ATEditorCharXScale / CharSize.XScaled) + 1;
  //use Round() to fix CudaText issue #4240

  ////this works
  ////a) better if clicked after line end, far
  ////b) bad if clicked exactly on line end (shifted to right by 1)
  //Result:= ListEnds.Len + (APixelsFromLeft - ListEnds.Data[ListEnds.Len-1] - ACharSize div 2) div ACharSize + 2;

  if not AAllowVirtualPos then
    Result:= Min(Result, Length(Str)+1);
end;

procedure TATStringTabHelper.FindOutputSkipOffset(ALineIndex: integer;
  const S: atString;
  const AScrollPosSmooth: Int64;
  out ACharsSkipped: Int64;
  out ACellPercentsSkipped: Int64);
var
  Offsets: TATIntFixedArray;
  NCheckedOffset: Int64;
begin
  ACharsSkipped:= 0;
  ACellPercentsSkipped:= 0;
  if (S='') or (AScrollPosSmooth=0) then Exit;

  CalcCharOffsets(ALineIndex, S, Offsets);

  NCheckedOffset:= AScrollPosSmooth * 100 * ATEditorCharXScale div CharSize.XScaled;

  while (ACharsSkipped<Offsets.Len) and
    (Offsets.Data[ACharsSkipped] < NCheckedOffset) do
    Inc(ACharsSkipped);

  if (ACharsSkipped>0) then
    ACellPercentsSkipped:= Offsets.Data[ACharsSkipped-1];
end;

function SGetItem(var S: string; const ch: Char = ','): string;
var
  i: integer;
begin
  i:= Pos(ch, S);
  if i=0 then
  begin
    Result:= S;
    S:= '';
  end
  else
  begin
    Result:= Copy(S, 1, i-1);
    Delete(S, 1, i);
  end;
end;


procedure TrimStringList(L: TStringList);
begin
  //dont do "while", we need correct last empty lines
  if (L.Count>0) and (L[L.Count-1]='') then
    L.Delete(L.Count-1);
end;

function SStringHasEol(const S: atString): boolean;
begin
  Result:=
    (Pos(#10, S)>0) or
    (Pos(#13, S)>0);
end;

function SStringHasEol(const S: string): boolean;
begin
  Result:=
    (Pos(#10, S)>0) or
    (Pos(#13, S)>0);
end;

function TATStringTabHelper.SpacesToTabs(ALineIndex: integer; const S: atString): atString;
begin
  Result:= StringReplace(S, StringOfCharW(' ', TabSize), WideChar(9), [rfReplaceAll]);
end;

function TATStringTabHelper.CharPosToColumnPos(ALineIndex: integer; const S: atString;
  APos: integer): integer;
begin
  if APos>Length(S) then
    Result:= TabsToSpaces_Length(ALineIndex, S, -1) + APos-Length(S)
  else
    Result:= TabsToSpaces_Length(ALineIndex, S, APos);
end;

function TATStringTabHelper.ColumnPosToCharPos(ALineIndex: integer; const S: atString;
  AColumn: integer): integer;
var
  size, i: integer;
begin
  if AColumn=0 then exit(AColumn);
  if not SStringHasTab(S) then exit(AColumn);

  size:= 0;
  for i:= 1 to Length(S) do
  begin
    if S[i]<>#9 then
      Inc(size)
    else
      Inc(size, CalcTabulationSize(ALineIndex, size+1));
    if size>=AColumn then
      exit(i);
  end;

  //column is too big, after line end
  Result:= AColumn - size + Length(S);
end;

function SStringHasTab(const S: atString): boolean;
begin
  Result:= Pos(#9, S)>0;
end;

function SStringHasTab(const S: string): boolean;
begin
  Result:= Pos(#9, S)>0;
end;


(*
function SStringHasAsciiAndNoTabs(const S: atString): boolean;
var
  code, i: integer;
begin
  for i:= 1 to Length(S) do
  begin
    code:= Ord(S[i]);
    if (code<32) or (code>=127) then
      exit(false);
  end;
  Result:= true;
end;

function SStringHasAsciiAndNoTabs(const S: string): boolean;
var
  code, i: integer;
begin
  for i:= 1 to Length(S) do
  begin
    code:= Ord(S[i]);
    if (code<32) or (code>=127) then
      exit(false);
  end;
  Result:= true;
end;
*)

function TATStringTabHelper.IndentUnindent(ALineIndex: integer; const Str: atString;
  ARight: boolean): atString;
var
  StrIndent, StrText: atString;
  DecSpaces, N: integer;
  DoTabs: boolean;
begin
  Result:= Str;

  if IndentSize=0 then
  begin
    if TabSpaces then
      StrIndent:= StringOfCharW(' ', TabSize)
    else
      StrIndent:= #9;
    DecSpaces:= TabSize;
  end
  else
  if IndentSize>0 then
  begin
    //use spaces
    StrIndent:= StringOfCharW(' ', IndentSize);
    DecSpaces:= IndentSize;
  end
  else
  begin
    //indent<0 - use tabs
    StrIndent:= StringOfCharW(#9, Abs(IndentSize));
    DecSpaces:= Abs(IndentSize)*TabSize;
  end;

  if ARight then
    Result:= StrIndent+Str
  else
  begin
    N:= SGetIndentChars(Str);
    StrIndent:= Copy(Str, 1, N);
    StrText:= Copy(Str, N+1, MaxInt);
    DoTabs:= SStringHasTab(StrIndent);

    StrIndent:= TabsToSpaces(ALineIndex, StrIndent);
    if DecSpaces>Length(StrIndent) then
      DecSpaces:= Length(StrIndent);
    Delete(StrIndent, 1, DecSpaces);

    if DoTabs then
      StrIndent:= SpacesToTabs(ALineIndex, StrIndent);
    Result:= StrIndent+StrText;
  end;
end;


function SRemoveNewlineChars(const S: atString): atString;
var
  i: integer;
begin
  Result:= S;
  for i:= 1 to Length(Result) do
    if IsCharEol(Result[i]) then
      Result[i]:= ' ';
end;

function STrimAll(const S: unicodestring): unicodestring;
var
  Ofs, Len: sizeint;
begin
  len := Length(S);
  while (Len>0) and (IsCharSpace(S[Len])) do
   dec(Len);
  Ofs := 1;
  while (Ofs<=Len) and (IsCharSpace(S[Ofs])) do
    Inc(Ofs);
  result := Copy(S, Ofs, 1 + Len - Ofs);
end;

function STrimLeft(const S: unicodestring): unicodestring;
var
  i,l:sizeint;
begin
  l := length(s);
  i := 1;
  while (i<=l) and (IsCharSpace(s[i])) do
    inc(i);
  Result := copy(s, i, l);
end;

function STrimRight(const S: unicodestring): unicodestring;
var
  l:sizeint;
begin
  l := length(s);
  while (l>0) and (IsCharSpace(s[l])) do
    dec(l);
  result := copy(s,1,l);
end;

function SBeginsWith(const S, SubStr: UnicodeString): boolean;
var
  i: integer;
begin
  Result:= false;
  if S='' then exit;
  if SubStr='' then exit;
  if Length(SubStr)>Length(S) then exit;
  for i:= 1 to Length(SubStr) do
    if S[i]<>SubStr[i] then exit;
  Result:= true;
end;

function SBeginsWith(const S, SubStr: string): boolean;
var
  i: integer;
begin
  Result:= false;
  if S='' then exit;
  if SubStr='' then exit;
  if Length(SubStr)>Length(S) then exit;
  for i:= 1 to Length(SubStr) do
    if S[i]<>SubStr[i] then exit;
  Result:= true;
end;

function SEndsWith(const S, SubStr: UnicodeString): boolean;
var
  i, Offset: integer;
begin
  Result:= false;
  if S='' then exit;
  if SubStr='' then exit;
  Offset:= Length(S)-Length(SubStr);
  if Offset<0 then exit;
  for i:= 1 to Length(SubStr) do
    if S[i+Offset]<>SubStr[i] then exit;
  Result:= true;
end;

function SEndsWith(const S, SubStr: string): boolean;
var
  i, Offset: integer;
begin
  Result:= false;
  if S='' then exit;
  if SubStr='' then exit;
  Offset:= Length(S)-Length(SubStr);
  if Offset<0 then exit;
  for i:= 1 to Length(SubStr) do
    if S[i+Offset]<>SubStr[i] then exit;
  Result:= true;
end;

function SBeginsWith(const S: UnicodeString; ch: WideChar): boolean;
begin
  Result:= (S<>'') and (S[1]=ch);
end;

function SBeginsWith(const S: string; ch: char): boolean;
begin
  Result:= (S<>'') and (S[1]=ch);
end;

function SEndsWith(const S: UnicodeString; ch: WideChar): boolean;
begin
  Result:= (S<>'') and (S[Length(S)]=ch);
end;

function SEndsWith(const S: string; ch: char): boolean;
begin
  Result:= (S<>'') and (S[Length(S)]=ch);
end;

function SEndsWithEol(const S: string): boolean;
begin
  Result:= (S<>'') and IsCharEol(S[Length(S)]);
end;

function SEndsWithEol(const S: atString): boolean;
begin
  Result:= (S<>'') and IsCharEol(S[Length(S)]);
end;

function SCharUpper(ch: WideChar): WideChar;
begin
  Result := CharUpperArray[Ord(Ch)];
end;

function SCharLower(ch: WideChar): WideChar;
begin
  Result := CharLowerArray[Ord(Ch)];
end;


function SCaseTitle(const S, SNonWordChars: atString): atString;
var
  i: integer;
begin
  Result:= S;
  for i:= 1 to Length(Result) do
    if (i=1) or not IsCharWord(S[i-1], SNonWordChars) then
      Result[i]:= SCharUpper(Result[i])
    else
      Result[i]:= SCharLower(Result[i]);
end;

function SCaseInvert(const S: atString): atString;
var
  ch, ch_up: WideChar;
  i: integer;
begin
  Result:= S;
  for i:= 1 to Length(Result) do
  begin
    ch:= Result[i];
    ch_up:= SCharUpper(ch);
    if ch<>ch_up then
      Result[i]:= ch_up
    else
      Result[i]:= SCharLower(ch);
  end;
end;

function SCaseSentence(const S, SNonWordChars: atString): atString;
var
  dot: boolean;
  ch: WideChar;
  i: Integer;
begin
  Result:= S;
  dot:= True;
  for i:= 1 to Length(Result) do
  begin
    ch:= Result[i];
    if IsCharWord(ch, SNonWordChars) then
    begin
      if dot then
        Result[i]:= SCharUpper(ch)
      else
        Result[i]:= SCharLower(ch);
      dot:= False;
    end
    else
      if (ch = '.') or (ch = '!') or (ch = '?') then
        dot:= True;
  end;
end;

function StringOfCharW(ch: WideChar; Len: integer): UnicodeString;
var
  i: integer;
begin
  SetLength(Result{%H-}, Len);
  for i:= 1 to Len do
    Result[i]:= ch;
end;


procedure SReplaceAll(var S: string; const SFrom, STo: string);
begin
  S:= StringReplace(S, SFrom, STo, [rfReplaceAll]);
end;

procedure SDeleteFrom(var s: string; const SFrom: string);
var
  n: integer;
begin
  n:= Pos(SFrom, S);
  if n>0 then
    SetLength(S, n-1);
end;

procedure SDeleteFrom(var s: UnicodeString; const SFrom: UnicodeString);
var
  n: integer;
begin
  n:= Pos(SFrom, S);
  if n>0 then
    SetLength(S, n-1);
end;

procedure SDeleteFromEol(var S: string);
var
  i: integer;
  ch: char;
begin
  for i:= 1 to Length(S) do
  begin
    ch:= S[i];
    if (ch=#10) or (ch=#13) then
    begin
      SetLength(S, i-1);
      Exit;
    end;
  end;
end;

procedure SDeleteFromEol(var S: UnicodeString);
var
  i: integer;
  ch: WideChar;
begin
  for i:= 1 to Length(S) do
  begin
    ch:= S[i];
    if (ch=#10) or (ch=#13) then
    begin
      SetLength(S, i-1);
      Exit;
    end;
  end;
end;

procedure SAddStringToHistory(const S: string; List: TStrings; MaxItems: integer);
var
  n: integer;
begin
  if s<>'' then
  begin
    n:= List.IndexOf(s);
    if n>=0 then
      List.Delete(n);
    List.Insert(0, s);
  end;

  while List.Count>MaxItems do
    List.Delete(List.Count-1);
end;

procedure SClipboardCopy(AText: string; AClipboardObj: TClipboard=nil);
begin
  if AText='' then exit;
  if AClipboardObj=nil then
    AClipboardObj:= Clipboard;

  {$IFDEF LCLGTK2}
  //Workaround for Lazarus bug #0021453. LCL adds trailing zero to clipboard in Clipboard.AsText.
  AClipboardObj.Clear;
  AClipboardObj.AddFormat(PredefinedClipboardFormat(pcfText), AText[1], Length(AText));
  {$ELSE}
  AClipboardObj.AsText:= AText;
  {$ENDIF}
end;


function SFindCharCount(const S: string; ch: char): integer;
var
  i: integer;
begin
  Result:= 0;
  for i:= 1 to Length(S) do
    if S[i]=ch then
      Inc(Result);
end;

function SFindCharCount(const S: UnicodeString; ch: WideChar): integer;
var
  i: integer;
begin
  Result:= 0;
  for i:= 1 to Length(S) do
    if S[i]=ch then
      Inc(Result);
end;


function SFindRegexMatch(const Subject, Regex: UnicodeString; out MatchPos, MatchLen: integer): boolean;
var
  Obj: TRegExpr;
begin
  Result:= false;
  MatchPos:= 0;
  MatchLen:= 0;

  Obj:= TRegExpr.Create;
  try
    Obj.ModifierS:= false;
    Obj.ModifierM:= true;
    Obj.ModifierI:= false;
    Obj.Expression:= Regex;

    if Obj.Exec(Subject) then
    begin
      Result:= true;
      MatchPos:= Obj.MatchPos[0];
      MatchLen:= Obj.MatchLen[0];
    end;
  finally
    FreeAndNil(Obj);
  end;
end;

function SFindRegexMatch(const Subject, Regex: UnicodeString; GroupIndex: integer; ModS, ModI, ModM: boolean): UnicodeString;
var
  Obj: TRegExpr;
begin
  Result:= '';
  Obj:= TRegExpr.Create;
  try
    Obj.ModifierS:= ModS;
    Obj.ModifierM:= ModM;
    Obj.ModifierI:= ModI;
    Obj.Expression:= Regex;
    if Obj.Exec(Subject) then
      Result:= Obj.Match[GroupIndex];
  finally
    FreeAndNil(Obj);
  end;
end;


function SCountTextOccurrences(const SubStr, Str: UnicodeString): integer;
var
  Offset: integer;
begin
  Result:= 0;
  if (Str='') or (SubStr='') then exit;
  Offset:= PosEx(SubStr, Str, 1);
  while Offset<>0 do
  begin
    Inc(Result);
    Offset:= PosEx(SubStr, Str, Offset + Length(SubStr));
  end;
end;

function SCountTextLines(const Str, StrBreak: UnicodeString): integer;
begin
  Result:= SCountTextOccurrences(StrBreak, Str)+1;
  // ignore trailing EOL
  if Length(Str)>=Length(StrBreak) then
    if Copy(Str, Length(Str)-Length(StrBreak)+1, Length(StrBreak))=StrBreak then
      Dec(Result);
end;

procedure SSplitByChar(const S: string; Sep: char; out S1, S2: string);
var
  N: integer;
begin
  N:= Pos(Sep, S);
  if N=0 then
  begin
    S1:= S;
    S2:= '';
  end
  else
  begin
    S1:= Copy(S, 1, N-1);
    S2:= Copy(S, N+1, Length(S));
  end;
end;

function IsStringWithUnicode(const S: string): boolean;
var
  i: integer;
begin
  for i:= 1 to Length(S) do
    if Ord(S[i])>=128 then
      exit(true);
  Result:= false;
end;

function IsStringWithUnicode(const S: UnicodeString): boolean;
var
  i: integer;
begin
  for i:= 1 to Length(S) do
    if Ord(S[i])>=128 then
      exit(true);
  Result:= false;
end;

function HexDigitToInt(ch: char): integer;
begin
  case ch of
    '0'..'9':
      Result:= Ord(ch)-Ord('0');
    'a'..'f':
      Result:= Ord(ch)-Ord('a')+10;
    'A'..'F':
      Result:= Ord(ch)-Ord('A')+10;
    else
      Result:= 0;
  end;
end;


end.
