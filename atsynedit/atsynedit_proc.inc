const
  //under Mac don't use Ctrl key, use Meta key as default (e.g. Meta+C, Meta+A)
  ssXControl = {$ifndef darwin} ssCtrl {$else} ssMeta {$endif};

{ TATEditorCommandLog }

constructor TATEditorCommandLog.Create;
begin
  inherited;
  MaxCount:= 200;
end;

procedure TATEditorCommandLog.Add(ACode: integer;
  AInvoke: TATEditorCommandInvoke; const AText: string);
var
  Item: TATEditorCommandLogItem;
begin
  if MaxCount<=0 then exit;
  while Size()>=MaxCount do
    PopFront();

  Item.ItemCode:= ACode;
  Item.ItemInvoke:= AInvoke;
  //Item.ItemTicks:= GetTickCount64;
  Item.ItemText:= AText;
  PushBack(Item);
end;

{ TATFoldedMarks }

function TATFoldedMarks.FindByCoord(ACoord: TPoint): TATFoldedMark;
var
  Mark: TATFoldedMark;
  i: integer;
begin
  for i:= 0 to Count-1 do
  begin
    Mark:= Items[i];
    if PtInRect(Mark.Coord, ACoord) then
      exit(Mark);
  end;
  Result.InitNone;
end;

{ TATFoldedMark }

procedure TATFoldedMark.Init(const ACoord: TRect; ALineFrom, ALineTo: integer);
begin
  Coord:= ACoord;
  LineFrom:= ALineFrom;
  LineTo:= ALineTo;
end;

procedure TATFoldedMark.InitNone;
begin
  Init(Rect(0, 0, 0, 0), -1, -1);
end;

function TATFoldedMark.IsInited: boolean;
begin
  Result:= LineFrom>=0;
end;

class operator TATFoldedMark.=(const a, b: TATFoldedMark): boolean;
begin
  Result:= false;
end;

{ TATEditorScrollInfo }

procedure TATEditorScrollInfo.Clear;
begin
  NPos:= 0;
  NPosLast:= 1;
  NMax:= 1;
  NPage:= 1;
  NPixelOffset:= 0;
  CharSizeScaled:= 1*ATEditorCharXScale;
  SmoothPos:= 0;
  SmoothPosLast:= 1;
  SmoothMax:= 1;
  SmoothPage:= 1;
end;

procedure TATEditorScrollInfo.SetZero;
begin
  NPos:= 0;
  NPixelOffset:= 0;
  SmoothPos:= 0;
end;

procedure TATEditorScrollInfo.SetLast;
begin
  NPos:= NPosLast;
  NPixelOffset:= 0;
  SmoothPos:= SmoothPosLast;
end;

procedure TATEditorScrollInfo.SetNPos(const AValue: Int64);
begin
  if NPosInternal=AValue then Exit;
  NPosInternal:= AValue;
end;

function TATEditorScrollInfo.TopGapVisible: boolean;
begin
  Result:= NPos=-1;
end;

function TATEditorScrollInfo.TotalOffset: Int64;
begin
  Result:= NPos * CharSizeScaled div ATEditorCharXScale + NPixelOffset;
end;

class operator TATEditorScrollInfo.= (const A, B: TATEditorScrollInfo): boolean;
begin
  Result:=
    (A.Vertical=B.Vertical) and
    (A.NPos=B.NPos) and
    (A.NMax=B.NMax) and
    (A.NPage=B.NPage) and
    (A.NPixelOffset=B.NPixelOffset);
end;


function EditorDoubleBufferedNeeded: boolean;
begin
  Result:= WidgetSet.GetLCLCapability(lcCanDrawOutsideOnPaint) = LCL_CAPABILITY_YES;
end;


procedure InitEditorMouseActions(out M: TATEditorMouseActions; ANoCtrlClickForCaret: boolean);
var
  NCount: integer;
  //
  procedure Add(AId: TATEditorMouseAction; AState: TShiftState); inline;
  begin
    M[NCount].MouseActionId:= AId;
    M[NCount].MouseState:= AState;
    Inc(NCount);
  end;
  //
begin
  M:= nil;
  NCount:= 0;

  //realloc to some big length
  SetLength(M, 25);

  Add(cMouseActionClickSimple, [ssLeft]);
  Add(cMouseActionClickSimple, [ssLeft, ssMeta]); //like VSCode
  Add(cMouseActionClickRight, [ssRight]);
  Add(cMouseActionClickRight, [ssRight, ssMeta]); //like VSCode
  Add(cMouseActionClickAndSelNormalBlock, [ssLeft, ssShift]);
  Add(cMouseActionClickAndSelVerticalBlock, [ssLeft, ssAlt, ssShift]);

  if ANoCtrlClickForCaret then
    Add(cMouseActionClickSimple, [ssLeft, ssXControl])
  else
    Add(cMouseActionMakeCaret, [ssLeft, ssXControl]);

  Add(cMouseActionMakeCaretsColumn, [ssLeft, ssXControl, ssShift]);

  Add(cMouseActionClickMiddle, [ssMiddle]);
  Add(cMouseActionMakeCaret, [ssMiddle, ssXControl]);

  Add(cMouseActionClickSimple, [ssLeft, ssAlt]);
  Add(cMouseActionClickSimple, [ssLeft, ssAltGr]);
  Add(cMouseActionClickSimple, [ssLeft, ssAlt, ssXControl]);
  Add(cMouseActionClickSimple, [ssLeft, ssAlt, ssShift, ssXControl]);

  //realloc at end, to actual count
  SetLength(M, NCount);
end;


function EditorMouseActionId(const M: TATEditorMouseActions; State: TShiftState): TATEditorMouseAction;
var
  i: integer;
begin
  Result:= cMouseActionNone;
  if State=[] then exit;

  for i:= Low(M) to High(M) do
    if M[i].MouseState=State then
      exit(M[i].MouseActionId);
end;

function EditorIsPressedCtrl: boolean;
begin
  {$ifdef darwin}
  //on macOS use Command-key instead of Ctrl
  Result:=
    (GetKeyState(VK_LWIN)<0) or
    (GetKeyState(VK_RWIN)<0);
  {$else}
  Result:= GetKeyState(VK_CONTROL)<0;
  {$endif}
end;

function EditorLinkIsEmail(const S: string): boolean;
const
  MailPrefix = 'mailto:';
begin
  if SBeginsWith(S, MailPrefix) then
    Result:= true
  else
  if (Pos('://', S)=0) and (Pos('@', S)>0) and (Pos('.', S)>0) then
    Result:= true
  else
    Result:= false;
end;

procedure EditorOpenLink(const S: string);
const
  MailPrefix = 'mailto:';
begin
  if EditorLinkIsEmail(S) then
  begin
    if SBeginsWith(S, MailPrefix) then
      OpenURL(S)
    else
      OpenURL(MailPrefix+S);
  end
  else
  if SBeginsWith(S, 'www.') then
    OpenURL('http://'+S)
  else
  if SBeginsWith(S, 'ftp.') then
    OpenURL('ftp://'+S)
  else
    OpenURL(S);
end;

procedure SRemoveAsciiControlChars(var S: UnicodeString; AReplaceChar: Widechar);
var
  i: integer;
begin
  for i:= 1 to Length(S) do
    if IsCharAsciiControl(S[i]) then
      S[i]:= AReplaceChar;
end;

//it's like LCLProc.IsEditableTextKey() but changed to ignore Num-keys
function IsEditorHotkeyForText(Key: Word): Boolean;
begin
  Result:= (Ord(Key)<256) and
    (byte(key) in [
      VK_A..VK_Z,
      //VK_NUMPAD0..VK_DIVIDE,
      VK_0..VK_9,
      186..192,
      219..222
      ]);
end;

function IsPointsDiffByDelta(const P1, P2: TPoint; Delta: integer): boolean; inline;
begin
  Result:=
    (Abs(P1.X-P2.X)>=Delta) or
    (Abs(P1.Y-P2.Y)>=Delta);

end;

{-----------------------------}
type
  TATIntPair = record
    NFrom, NTo: integer;
  end;
  TATIntPairArray = array of TATIntPair;

function DivideRangeByPartsFrom0(N, Parts: integer): TATIntPairArray;
var
  i, d: integer;
begin
  if Parts<1 then
    Parts:= 1;
  if N<0 then
  begin
    SetLength(Result{%H-}, 0);
    exit;
  end;
  if N+1<=Parts then
  begin
    SetLength(Result, N+1);
    for i:= 0 to N do
    begin
      Result[i].NFrom:= i;
      Result[i].NTo:= i;
    end;
  end
  else
  begin
    SetLength(Result, Parts);
    Result[0].NFrom:= 0;
    Result[High(Result)].NTo:= N;
    for i:= 1 to Parts-1 do
    begin
      d:= N * i div Parts;
      Result[i-1].NTo:= d;
      Result[i].NFrom:= d+1;
    end;
  end;
end;

function DivideRangeByParts(NFrom, NTo, Parts: integer): TATIntPairArray;
var
  i: integer;
begin
  Result:= DivideRangeByPartsFrom0(NTo-NFrom, Parts);
  for i:= 0 to High(Result) do
  begin
    Inc(Result[i].NFrom, NFrom);
    Inc(Result[i].NTo, NFrom);
  end;
end;

procedure SwapPoints(var Pnt1, Pnt2: TPoint);
var
  Pnt3: TPoint;
begin
  Pnt3:= Pnt1;
  Pnt1:= Pnt2;
  Pnt2:= Pnt3;
end;

