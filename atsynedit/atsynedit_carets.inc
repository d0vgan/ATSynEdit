{$ifdef nnnn}begin end;{$endif}

function TATSynEdit.IsLineWithCaret(ALine: integer; ADisableSelected: boolean=false): boolean; inline;
begin
  Result:= FCarets.IsLineWithCaret(ALine, ADisableSelected);
end;

function TATSynEdit.IsWrapItemWithCaret(constref AWrapItem: TATWrapItem): boolean;
var
  Caret: TATCaretItem;
  NLineIndex, NFirstCaret, i: integer;
begin
  Result:= false;
  NLineIndex:= AWrapItem.NLineIndex;
  NFirstCaret:= Carets.FindCaretBeforePos(0, NLineIndex, false);
  for i:= Max(0, NFirstCaret) to Carets.Count-1 do
  begin
    Caret:= Carets[i];
    if Caret.PosY>NLineIndex then
      exit;
    if AWrapItem.ContainsPos(Caret.PosX, Caret.PosY) then
      exit(true);
  end;
end;


procedure TATSynEdit.DoCaretsAddOnColumnBlock(APos1, APos2: TPoint; const ARect: TRect);
var
  St: TATStrings;
  i: integer;
begin
  St:= Strings;
  for i:= APos1.Y to APos2.Y do
  begin
    if i=APos1.Y then
      Carets.Clear;
    Carets.Add(
      St.ColumnPosToCharPos(i, ARect.Right, FTabHelper),
      i,
      St.ColumnPosToCharPos(i, ARect.Left, FTabHelper),
      i
      );
  end
end;

procedure TATSynEdit.DoCaretAddToPoint(AX, AY: integer);
var
  N: integer;
begin
  N:= Carets.IndexOfPosXY(AX, AY);
  if Carets.IsIndexValid(N) then
  begin
    if Carets.Count>1 then
      Carets.Delete(N);
  end
  else
  begin
    Carets.Add(AX, AY);
  end;
end;

procedure TATSynEdit.DoCaretsColumnToPoint(AX, AY: integer);
var
  PCaret, PCoord1, PCoord2: TPoint;
  Details: TATEditorPosDetails;
begin
  //needed to Undo multi-carets to last multi-caret state, before single caret appears
  Strings.SetGroupMark;

  DoCaretSingleAsIs;
  with Carets[0] do
  begin
    PCaret.X:= PosX;
    PCaret.Y:= PosY;
  end;

  PCoord1:= CaretPosToClientPos(PCaret);
  PCoord2:= CaretPosToClientPos(Point(AX, AY));

  //clicked above caret?
  if PCoord2.Y<PCoord1.Y then
    SwapInt(PCoord1.Y, PCoord2.Y);

  Carets.Clear;
  PCaret:= ClientPosToCaretPos(PCoord1, Details);
  if PCaret.Y>=0 then
    Carets.Add(PCaret.X, PCaret.Y);

  repeat
    Inc(PCoord1.Y, FCharSize.Y);
    PCaret:= ClientPosToCaretPos(PCoord1, Details);
    if PCaret.Y>=0 then
      if not Carets.IsLineWithCaret(PCaret.Y) then
        Carets.Add(PCaret.X, PCaret.Y);
  until PCoord1.Y>=PCoord2.Y;

  if Carets.Count=0 then
    Carets.Add(AX, AY);
end;

function TATSynEdit.DoCalcForegroundFromAttribs(AX, AY: integer; var AColor: TColor; var AFontStyles: TFontStyles): boolean;
var
  Marker: PATMarkerItem;
  NMarkerIndex: integer;
begin
  Result:= false;
  if Assigned(Attribs) and (Attribs.Count>0) then
  begin
    NMarkerIndex:= Attribs.FindContaining(AX, AY);
    if NMarkerIndex>=0 then
    begin
      Result:= true;
      Marker:= Attribs.ItemPtr(NMarkerIndex);
      AColor:= Marker^.LinePart.ColorFont;
      AFontStyles:= ConvertIntegerToFontStyles(Marker^.LinePart.FontStyles);
    end;
  end;
end;

procedure TATSynEdit.UpdateCaretsCoords(AOnlyLast: boolean=false; ASkipInvisible: boolean=false);
{
UpdateCaretsCoords is called 2 times per each keypress. too often? no, cannot call it less.
a) UpdateCaretsCoords is called from DoCommand (which calls DoCommandResults). call is required for
  CudaText macros: macro can run N commands, each command can scroll the editor, so UpdateCaretsCoords
  call is needed to do the carets correct moving from macros.
b) UpdateCaretsCoords is called from DoPaintAll. it is not related to DoCommand/DoCommandResults,
  repaint occurs from user mouse actions, scrollbar moving.
}
var
  St: TATStrings;
  Caret: TATCaretItem;
  PntCaret, PntCoord: TPoint;
  NCaretCount, NStart: integer;
  //NLineTop, NLineBottom: integer;
  iCaret: integer;
begin
  if Assigned(FOnCalcCaretsCoords) then
    FOnCalcCaretsCoords(Self);

  St:= Strings;
  NCaretCount:= Carets.Count;
  if AOnlyLast then
    NStart:= NCaretCount-1
  else
    NStart:= 0;

  //fixing CudaText #4427, always update all carets
  {
  if ASkipInvisible then
  begin
    NLineTop:= LineTop;
    NLineBottom:= LineBottom;
  end;
  }

  for iCaret:= NStart to NCaretCount-1 do
  begin
    Caret:= Carets[iCaret];
    Caret.CoordX:= -1;
    Caret.CoordY:= -1;

    PntCaret.X:= Caret.PosX;
    PntCaret.Y:= Caret.PosY;

    if not St.IsIndexValid(PntCaret.Y) then
      Continue;

    {
    //fixing CudaText #4427
    if ASkipInvisible then
    begin
      if PntCaret.Y<NLineTop then Continue;
      if PntCaret.Y>NLineBottom then Continue;
    end;
    }

    if not IsPosFolded(PntCaret.X, PntCaret.Y) then
    begin
      PntCoord:= CaretPosToClientPos(PntCaret);
      Caret.CoordX:= PntCoord.X;
      Caret.CoordY:= PntCoord.Y;
    end;

    if ATEditorOptions.CaretTextOverInvertedRect then
    begin
      Caret.CharStr:= '';
      Caret.CharColor:= clNone;
      Caret.CharStyles:= [];
      if Caret.PosX<St.LinesLen[Caret.PosY] then
      begin
        Caret.CharStr:= St.LineSub(Caret.PosY, Caret.PosX+1, 1);

        if not DoCalcForegroundFromAttribs(
                 Caret.PosX,
                 Caret.PosY,
                 Caret.CharColor,
                 Caret.CharStyles) then
          if Assigned(AdapterForHilite) then
            AdapterForHilite.OnEditorCalcPosForeground(nil,
              Caret.PosX,
              Caret.PosY,
              Caret.CharColor,
              Caret.CharStyles
              );
      end;
    end;
  end;
end;

procedure TATSynEdit.UpdateMarkersCoords;
var
  Marker: PATMarkerItem;
  PntCaret, PntCoord: TPoint;
  NLineTop, NLineBottom: integer;
  i: integer;
begin
  if FMarkers=nil then exit;

  NLineTop:= LineTop;
  NLineBottom:= LineBottom;

  for i:= 0 to FMarkers.Count-1 do
  begin
    Marker:= FMarkers.ItemPtr(i);
    Marker^.CoordX:= -1;
    Marker^.CoordY:= -1;
    Marker^.CoordX2:= -1;
    Marker^.CoordY2:= -1;

    if Marker^.PosY<NLineTop then Continue;
    if Marker^.PosY>NLineBottom then Continue;

    PntCaret.X:= Marker^.PosX;
    PntCaret.Y:= Marker^.PosY;

    if not IsPosFolded(PntCaret.X, PntCaret.Y) then
    begin
      PntCoord:= CaretPosToClientPos(PntCaret);
      Marker^.CoordX:= PntCoord.X;
      Marker^.CoordY:= PntCoord.Y;
      if Marker^.LineLen<>0 then
      begin
        PntCaret.X:= Marker^.PosX+Marker^.LineLen;
        PntCaret.Y:= Marker^.PosY;
        PntCoord:= CaretPosToClientPos(PntCaret);
        Marker^.CoordX2:= PntCoord.X;
        Marker^.CoordY2:= PntCoord.Y;
      end;
    end;
  end;
end;


function _DoCaretPosToClientPos(
  P: TPoint;
  AWrapInfo: TATWrapInfo;
  AStrings: TATStrings;
  AGaps: TATGaps;
  ATabHelper: TATStringTabHelper;
  const ARect: TRect;
  const AScrollHorz, AScrollVert: TATEditorScrollInfo;
  APreferLeftSide: boolean): TPoint;
var
  Item: TATWrapItem;
  StringItem: PATStringItem;
  NIndex1, NIndex2, NLineIndexTop: integer;
  NFromStart, NWidth: Int64;
  NScrolledHorzSmooth: Int64;
  NScrolledVert: Int64;
  NLineCount: integer;
  Str: atString;
  bLineHuge: boolean;
  bAsciiOnly: boolean;
  bAfterEnd: boolean;
  i: integer;
begin
  Result.X:= -1;
  Result.Y:= -1;

  if P.Y<0 then Exit;

  NLineCount:= AStrings.Count;
  if NLineCount=0 then
  begin
    Result.X:= ARect.Left;
    Result.Y:= ARect.Top;
    Exit;
  end;

  //if needed pos after file-end, we
  //1) calculate pos of last char, 2) increase Y by char height
  bAfterEnd:= P.Y>=NLineCount;
  if bAfterEnd then
  begin
    P.Y:= NLineCount-1;
    P.X:= AStrings.LinesLen[P.Y];
  end;

  AWrapInfo.FindIndexesOfLineNumber(P.Y, NIndex1, NIndex2);
  if NIndex1<0 then Exit;

  //prepare optimization
  StringItem:= AStrings.GetItemPtr(P.Y);
  bAsciiOnly:= not ATabHelper.FontProportional and StringItem^.HasAsciiNoTabs;

  NScrolledHorzSmooth:= AScrollHorz.SmoothPos-AScrollHorz.NPixelOffset;
  NScrolledVert:= Max(0, AScrollVert.NPos);

  for i:= NIndex1 to NIndex2 do
  begin
    Item:= AWrapInfo[i];
    bLineHuge:= Item.NLength>ATEditorOptions.MaxLineLenForAccurateCharWidths;

    if (P.X<Item.NCharIndex-1) then Continue;
    if (Item.NFinal=cWrapItemMiddle) then
      if (P.X>Item.NCharIndex-1+Item.NLength) or
        ((P.X=Item.NCharIndex-1+Item.NLength) and APreferLeftSide) then
        Continue;

    NFromStart:= P.X+1-Item.NCharIndex;

    if bAsciiOnly or bLineHuge then
    begin
      NWidth:= Min(NFromStart, Item.NLength) * ATabHelper.CharSize.XScaled div ATEditorCharXScale;
    end
    else
    begin
      Str:= AStrings.LineSub(P.Y, Item.NCharIndex, Min(NFromStart, Item.NLength));
      NWidth:= CanvasTextWidth(Str, P.Y, ATabHelper, ATabHelper.CharSize.XScaled, ATabHelper.FontProportional) div ATEditorCharXScale;
    end;

    Result.X:= NWidth;

    if NFromStart>Item.NLength then
      Inc(Result.X, (NFromStart-Item.NLength)*ATabHelper.CharSize.XScaled div ATEditorCharXScale);

    Inc(Result.X, Item.NIndent*ATabHelper.CharSize.XScaled*ATabHelper.CharSize.XSpacePercents div ATEditorCharXScale div 100 - NScrolledHorzSmooth);
    Result.Y:= (i-NScrolledVert)*ATabHelper.CharSize.Y;

    //consider gaps
    if (AGaps.Count>0) and AWrapInfo.IsIndexValid(NScrolledVert) then
    begin
      NLineIndexTop:= AWrapInfo[NScrolledVert].NLineIndex;
      Inc(Result.Y, AGaps.SizeForLineRange(
        NLineIndexTop, //index of top line
        P.Y-1 //index of line before caret
        ));
      //consider top gap
      if AScrollVert.TopGapVisible then
        Inc(Result.Y, AGaps.SizeOfGapTop);
    end;

    //coords of corner
    Inc(Result.X, ARect.Left);
    Inc(Result.Y, ARect.Top);

    if bAfterEnd then
    begin
      Result.X:= ARect.Left;
      Inc(Result.Y, ATabHelper.CharSize.Y);
    end;

    Exit
  end;
end;


function _DoClientPosToCaretPos(
  P: TPoint;
  AWrapInfo: TATWrapInfo;
  AStrings: TATStrings;
  AGaps: TATGaps;
  ATabHelper: TATStringTabHelper;
  const ARect: TRect;
  const AScrollHorz, AScrollVert: TATEditorScrollInfo;
  AVirtualPos: boolean;
  out ADetails: TATEditorPosDetails;
  AGapCoordAction: TATEditorGapCoordAction;
  AAfterEofUsesLastLen: boolean;
  AEditorIndex: integer): TPoint;
var
  CharOffsets: TATIntFixedArray;
  NLineLenOriginal, NLineLenTrimmed: integer;
  NIndex, NIndexDecrement, NPixels: Int64;
  NGapObjectWidth: integer;
  Item: TATWrapItem;
  ItemGap: TATGapItem;
  Str: atString;
  AllowVirtual, bGapGoesToEnd: boolean;
begin
  Result.X:= 0;
  Result.Y:= -1;
  ADetails:= Default(TATEditorPosDetails);

  if (ATabHelper.CharSize.XScaled div ATEditorCharXScale <=0) or (ATabHelper.CharSize.Y<=0) then Exit;
  P.X:= Max(P.X, ARect.Left);

  //find index in WrapInfo for P.Y
  NPixels:= ARect.Top;
  NIndex:= Max(0, AScrollVert.NPos);
  bGapGoesToEnd:= false;

  //allow to work for arrow-up/ page-up, if caret at top
  if P.Y<ARect.Top then
    if AWrapInfo.IsIndexValid(NIndex) then
    begin
      //start later loop, but from decremented NIndex
      //todo: this not correct if upper (not visible) lines have gaps...
      NIndexDecrement:= Min((ARect.Top-P.Y) div ATabHelper.CharSize.Y + 1, NIndex);
      Dec(NIndex, NIndexDecrement);
      Dec(NPixels, NIndexDecrement*ATabHelper.CharSize.Y);
    end;

  //support gap before 1st line
  if (NIndex=0) and AScrollVert.TopGapVisible and (AGaps.Count>0) then
    Inc(NPixels, AGaps.SizeOfGapTop);

  repeat
    if not AWrapInfo.IsIndexValid(NIndex) then
      Break;

    Inc(NPixels, ATabHelper.CharSize.Y);
    if NPixels>P.Y then Break;

    //consider gap
    Item:= AWrapInfo[NIndex];
    if (Item.NFinal=cWrapItemFinal) and (AGaps.Count>0) then
    begin
      ItemGap:= AGaps.Find(Item.NLineIndex);
      if Assigned(ItemGap) then
      begin
        Inc(NPixels, ItemGap.Size);
        if NPixels>P.Y then
        begin
          ADetails.OnGapItem:= ItemGap;
          ADetails.OnGapPos.Y:= P.Y - (NPixels-ItemGap.Size);
          NGapObjectWidth:= ItemGap.GetObjectWidth;
          if NGapObjectWidth>0 then
            ADetails.OnGapPos.X:= P.X - GetGapBitmapPosLeft(ARect, NGapObjectWidth)
          else
            ADetails.OnGapPos.X:= P.X - ARect.Left;

          case AGapCoordAction of
            cGapCoordToLineEnd:
              bGapGoesToEnd:= true;
            cGapCoordMoveDown:
              begin
                if AWrapInfo.IsIndexValid(NIndex+1) then
                  Inc(NIndex);
              end;
          end;
          Break;
        end;
      end;
    end;

    Inc(NIndex);
  until false;

  //click above all text
  if NIndex<0 then
  begin
    Result.X:= 0;
    Result.Y:= 0;
    Exit
  end;

  //click below all text
  if not AWrapInfo.IsIndexValid(NIndex) then
  begin
    NIndex:= AWrapInfo.Count-1;
    if AAfterEofUsesLastLen then
    begin
      if AWrapInfo.IsIndexValid(NIndex) then
      begin
        Item:= AWrapInfo[NIndex];
        Result.Y:= Item.NLineIndex;
        Result.X:= Item.NCharIndex+Item.NLength-1;
        if not AStrings.IsIndexValid(Result.Y) then
          exit(Point(-1, -1));
      end;
      Exit
    end;
  end;

  //click in text
  Item:= AWrapInfo[NIndex];
  Result.Y:= Item.NLineIndex;
  if not AStrings.IsIndexValid(Result.Y) then
    exit(Point(-1, -1));

  NLineLenOriginal:= Item.NLength;
  if ATabHelper.FontProportional then
    NLineLenTrimmed:= Min(Item.NLength, (AScrollHorz.SmoothPos + ARect.Width) div 2) //use min char width 2px
  else
    NLineLenTrimmed:= Min(Item.NLength, (AScrollHorz.SmoothPos + ARect.Width) * ATEditorCharXScale div ATabHelper.CharSize.XScaled);

  if NLineLenOriginal>ATEditorOptions.MaxLineLenForAccurateCharWidths then
  begin
    Str:= StringOfCharW(ATEditorOptions.SampleChar, NLineLenOriginal); //fake to calc CharOffsets only
    ATabHelper.CalcCharOffsets(Item.NLineIndex, Str, CharOffsets, 0);
    Str:= AStrings.LineSub(Item.NLineIndex, Item.NCharIndex, NLineLenTrimmed);
  end
  else
  begin
    Str:= AStrings.LineSub(Item.NLineIndex, Item.NCharIndex, NLineLenTrimmed);
    ATabHelper.CalcCharOffsets(Item.NLineIndex, Str, CharOffsets, 0);
  end;

  if bGapGoesToEnd then
    Result.X:= Length(Str) + Item.NCharIndex-1
  else
  begin
    AllowVirtual:= AVirtualPos and (Item.NFinal=cWrapItemFinal);
    NPixels:= Int64(P.X)-ARect.Left + AScrollHorz.SmoothPos - ATabHelper.CharSize.XScaled*ATabHelper.CharSize.XSpacePercents*Item.NIndent div ATEditorCharXScale div 100;
    Result.X:= ATabHelper.FindClickedPosition(
      Result.Y,
      Str,
      CharOffsets,
      NPixels,
      AllowVirtual,
      ADetails.EndOfWrappedLine)
      + Item.NCharIndex - 2;

    //position after end of partially folded line? return position of fold begin
    if ADetails.EndOfWrappedLine then
    begin
      NIndex:= AStrings.LinesFoldFrom[Result.Y, AEditorIndex];
      if NIndex>0 then
        Result.X:= NIndex-1;
    end;
  end;

  if Item.NFinal=cWrapItemFinal then //don't set AWrappedEnd for real eol
    ADetails.EndOfWrappedLine:= false;
end;

function TATSynEdit.CaretPosToClientPos(P: TPoint): TPoint;
begin
  Result:= _DoCaretPosToClientPos(P,
    FWrapInfo,
    Strings,
    Gaps,
    FTabHelper,
    FRectMain,
    FScrollHorz,
    FScrollVert,
    FCaretSpecPos or FOptCaretPreferLeftSide
    );

  if Result.Y>=0 then
    Inc(Result.Y, FTextOffsetFromTop1);
end;

function TATSynEdit.ClientPosToCaretPos(P: TPoint;
  out ADetails: TATEditorPosDetails;
  AGapCoordAction: TATEditorGapCoordAction = cGapCoordToLineEnd): TPoint;
begin
  Dec(P.Y, FTextOffsetFromTop1);

  Result:= _DoClientPosToCaretPos(P,
    FWrapInfo,
    Strings,
    Gaps,
    FTabHelper,
    FRectMain,
    FScrollHorz,
    FScrollVert,
    FCaretVirtual or FMouseDownAndColumnSelection,
    ADetails,
    AGapCoordAction,
    IsSelRectEmpty,
    FEditorIndex);
end;

procedure TATSynEdit.InitLengthArray(out Lens: TATIntArray);
var
  St: TATStrings;
  i: integer;
begin
  St:= Strings;
  SetLength(Lens{%H-}, St.Count);
  for i:= 0 to Length(Lens)-1 do
    Lens[i]:= St.LinesLen[i];
end;

function TATSynEdit.OffsetToCaretPos(const APos: integer): TPoint;
var
  Buffer: TATStringBuffer;
  Lens: TATIntArray;
begin
  Buffer:= TATStringBuffer.Create;
  try
    InitLengthArray(Lens);
    Buffer.Setup('?', Lens);
    Result:= Buffer.StrToCaret(APos);
  finally
    FreeAndNil(Buffer);
  end;
end;

function TATSynEdit.CaretPosToOffset(const ACaret: TPoint): integer;
var
  Buffer: TATStringBuffer;
  Lens: TATIntArray;
begin
  Buffer:= TATStringBuffer.Create;
  try
    InitLengthArray(Lens);
    Buffer.Setup('?', Lens);
    Result:= Buffer.CaretToStr(ACaret);
  finally
    FreeAndNil(Buffer);
  end;
end;

procedure TATSynEdit.SetCaretBlinkEnabled(AValue: boolean);
begin
  if FCaretBlinkEnabled=AValue then Exit;
  FCaretBlinkEnabled:= AValue;
  TimerBlinkEnable;
end;

procedure TATSynEdit.DoScroll_IndentFromTop(AWrapInfoIndex, AIndentVert: integer); inline;
begin
  AIndentVert:= Min(AIndentVert, GetVisibleLines-1);
  FScrollVert.NPos:= Max(0, AWrapInfoIndex-AIndentVert);
  FScrollVert.NPixelOffset:= 0;
end;

function TATSynEdit.DoScroll_IndentFromBottom(AWrapInfoIndex, AIndentVert: integer): boolean;
//index: original vert scroll pos
//do dec(index), until offset (pixels) from caret to bottom is small enough
var
  NPixels, NPixelsNeeded, NIndex, NCell: integer;
  ItemGap: TATGapItem;
  i: integer;
begin
  Result:= false;
  NIndex:= AWrapInfoIndex;
  if NIndex<=0 then exit;

  NCell:= FCharSize.Y;
  NPixels:= FRectMain.Height-NCell;
  if NPixels<=0 then exit;
    //this is distance of first visible line, to bottom of control
  NPixelsNeeded:= (AIndentVert+1)*NCell-1;
    //this distance must be reached,
    //can be smaller/bigger, if wrapped lines or gaps

  for i:= 1 to GetVisibleLines do
  begin
    Dec(NIndex);
    if NIndex<0 then
    begin
      FScrollVert.SetZero;
      Exit(true);
    end;

    Dec(NPixels, NCell);

    //consider gap
    if FWrapInfo[NIndex].NFinal=cWrapItemFinal then
    begin
      ItemGap:= Gaps.Find(FWrapInfo[NIndex].NLineIndex);
      if Assigned(ItemGap) then
        Dec(NPixels, ItemGap.Size);
    end;

    //support gap before 1st line
    if (NIndex=0) then
    begin
      ItemGap:= Gaps.Find(-1);
      if Assigned(ItemGap) then
        Dec(NPixels, ItemGap.Size);
    end;

    if NPixels<0 then
    begin
      FScrollVert.NPos:= NIndex+1;
      Exit(true);
    end;

    if NPixels<=NPixelsNeeded then
    begin
      FScrollVert.NPos:= NIndex;
      Exit(true);
    end;
  end;
end;

function TATSynEdit.DoCaretApplyProximityToVertEdge(ACaretPos: TPoint; ACaretCoordY: integer;
  AProximity, AIndentVert: integer): boolean;
//  vert indent <0: always indent from top;
//             >=0: indent from top (goto up) or bottom (goto down)
var
  NIndex: integer;
begin
  Result:= false;
  if ACaretCoordY < FRectMain.Top + AProximity*FCharSize.Y then
  begin
    NIndex:= FWrapInfo.FindIndexOfCaretPos(ACaretPos);
    if NIndex<0 then Exit;
    DoScroll_IndentFromTop(NIndex, Abs(AIndentVert));
    Result:= true;
  end
  else
  if ACaretCoordY+FCharSize.Y >= FRectMain.Bottom - AProximity*FCharSize.Y then
  begin
    NIndex:= FWrapInfo.FindIndexOfCaretPos(ACaretPos);
    if NIndex<0 then Exit;
    if AIndentVert<0 then
      DoScroll_IndentFromTop(NIndex, Abs(AIndentVert))
    else
      DoScroll_IndentFromBottom(NIndex, Abs(AIndentVert));
    Result:= true;
  end;
end;

function TATSynEdit.DoCaretApplyProximityToHorzEdge(ACaretCoordX, AProximity, AIndentHorz: integer): boolean;
var
  NChars: Int64;
begin
  Result:= false;

  //not yet inited FRectMain? do exit
  if FRectMain=cRectEmpty then
  begin
    exit;
  end;

  if ACaretCoordX < FRectMain.Left + AProximity*FCharSize.XScaled div ATEditorCharXScale then
  begin
    NChars:= Int64(FRectMain.Left-ACaretCoordX) * ATEditorCharXScale div FCharSize.XScaled + 1 + AIndentHorz;
    FScrollHorz.NPos:= Max(0, FScrollHorz.NPos-NChars);
    Result:= true;
  end
  else
  if ACaretCoordX > FRectMain.Right - (AProximity+1)*FCharSize.XScaled div ATEditorCharXScale then
  begin
    NChars:= Int64(ACaretCoordX-FRectMain.Right) * ATEditorCharXScale div FCharSize.XScaled + 2 + AIndentHorz;
    FScrollHorz.NPos:= FScrollHorz.NPos+NChars;
    Result:= true;
  end;
end;

function TATSynEdit.DoShowPos(const APos: TPoint; AIndentHorz, AIndentVert: integer;
  AUnfold, AllowUpdate, AllowProximity: boolean): boolean;
var
  Coord: TPoint;
  NProximityVert, NProximityHorz: integer;
begin
  Result:= false;

  //allow big values - make centering vertically
  if Abs(AIndentVert)>=100 then
    AIndentVert:= GetVisibleLines div 2;

  if AUnfold then
    if IsLineFolded(APos.Y, true) then
      DoUnfoldLine(APos.Y);

  Coord:= CaretPosToClientPos(APos);

  if AllowProximity then
  begin
    NProximityVert:= GetActualProximityVert;
    NProximityHorz:= 0;
  end
  else
  begin
    NProximityVert:= 0;
    NProximityHorz:= 0;
  end;

  //for y
  if DoCaretApplyProximityToVertEdge(APos, Coord.Y, NProximityVert, AIndentVert) then
    Result:= true;

  //for x
  if DoCaretApplyProximityToHorzEdge(Coord.X, NProximityHorz, AIndentHorz) then
    Result:= true;

  if Result then
  begin
    UpdateScrollbars(true);
    if AllowUpdate then
      Update(false, true{AForceRepaint});
  end;
end;

procedure TATSynEdit.DoGotoPos(const APos, APosEnd: TPoint;
  AIndentHorz, AIndentVert: integer;
  APlaceCaret, ADoUnfold: boolean;
  AAllowProcessMsg: boolean=true;
  AAllowUpdate: boolean=true;
  AAllowProximity: boolean=true);
var
  changed1, changed2: boolean;
begin
  if not Strings.IsIndexValid(APos.Y) then exit;

  //fixing CudaText #4383
  //if called with not yet painted control, we should not calculate any coords,
  //should not update scrollbars, etc
  if (not FPaintStarted) or (FRectMain=cRectEmpty) then
  begin
    if APlaceCaret then
      DoCaretSingle(APos.X, APos.Y, APosEnd.X, APosEnd.Y);
    exit;
  end;

  //if IsPosFolded(APos.X, APos.Y) - better?
  if IsLineFolded(APos.Y, true) then
  begin
    if ADoUnfold then
    begin
      DoUnfoldLine(APos.Y);
      if AAllowUpdate then
        Update;
      if AAllowProcessMsg then
        Application.ProcessMessages;
    end
    else
      exit;
  end;

  if APlaceCaret then
  begin
    DoCaretSingle(APos.X, APos.Y, APosEnd.X, APosEnd.Y);
    ActionAddJumpToUndo;
    DoEventCarets;
  end;

  if APosEnd.Y>=0 then
    changed1:= DoShowPos(APosEnd, AIndentHorz, AIndentVert, ADoUnfold, false, AAllowProximity)
  else
    changed1:= false;
  changed2:= DoShowPos(APos, AIndentHorz, AIndentVert, ADoUnfold, false, AAllowProximity);

  ////always update scrollbar after all keyboard cmds (PageUp, PageDown, Ctrl+Home)
  if not changed1 and not changed2 then
    UpdateScrollbars(true);

  if AAllowUpdate then
    Update(false, true{AForceRepaint});
end;

procedure TATSynEdit.DoGotoCaret(AEdge: TATCaretEdge; AUndoRedo: boolean=false;
  AAllowProcessMsg: boolean=true; AAllowUpdate: boolean=true;
  AAllowProximity: boolean=true);
var
  NIndentVert, NIndentHorz: integer;
begin
  if AUndoRedo then
  begin
    NIndentVert:= FOptUndoIndentVert;
    NIndentHorz:= FOptUndoIndentHorz;
  end
  else
  begin
    NIndentVert:= FOptScrollIndentCaretVert;
    NIndentHorz:= FOptScrollIndentCaretHorz;
  end;

  DoGotoPos(
    Carets.CaretAtEdge(AEdge),
    Point(-1, -1),
    NIndentHorz,
    NIndentVert,
    false,
    false, //dont unfold
    AAllowProcessMsg,
    AAllowUpdate,
    AAllowProximity
    );
end;


procedure TATSynEdit.DoCaretsDeleteOnSameLines;
var
  i: integer;
begin
  for i:= Carets.Count-1 downto 1{!} do
  begin
    if Carets[i].PosY=Carets[i-1].PosY then
      Carets.Delete(i);
  end;
end;

procedure TATSynEdit.DoCaretSingleAsIs;
var
  i: integer;
begin
  FSelRect:= cRectEmpty;

  if Carets.Count=0 then
    Carets.Add(0, 0)
  else
  for i:= Carets.Count-1 downto 1{not 0} do
    Carets.Delete(i);
end;


procedure TATSynEdit.DoCaretSingle(APosX, APosY, AEndX, AEndY: integer);
begin
  FSelRect:= cRectEmpty;
  Carets.Clear;
  Carets.Add(APosX, APosY, AEndX, AEndY);
end;

procedure TATSynEdit.DoCaretSingle(AX, AY: integer; AClearSelection: boolean);
var
  NEndX, NEndY: integer;
  Caret: TATCaretItem;
begin
  NEndX:= -1;
  NEndY:= -1;

  if (not AClearSelection) and (Carets.Count>0) then
  begin
    Caret:= Carets[0];
    NEndX:= Caret.EndX;
    NEndY:= Caret.EndY;
  end;

  DoCaretSingle(AX, AY, NEndX, NEndY);
end;

function TATSynEdit.DoCaretSwapEdge(Item: TATCaretItem; AMoveLeft: boolean): boolean;
var
  X1, Y1, X2, Y2: integer;
  bSel, bAtLeft: boolean;
begin
  Result:= false;
  Item.GetRange(X1, Y1, X2, Y2, bSel);
  if not bSel then Exit;
  Result:= true;

  bAtLeft:= not Item.IsForwardSelection;

  //Left/Rt pressed at left/rt side of selection?
  //yes: cancel selection, don't move caret
  if bAtLeft=AMoveLeft then
  begin
    Item.EndX:= -1;
    Item.EndY:= -1;
    Exit
  end;

  //else swap edge
  Item.SwapSelection;

  if not FOptKeyLeftRightSwapSelAndSelect then
  begin
    Item.EndX:= -1;
    Item.EndY:= -1;
  end;
end;

function TATSynEdit.GetCaretsArray: TATPointArray;
begin
  SetLength(Result{%H-}, 0);
  if Assigned(FCarets) then
    Result:= FCarets.AsArray;
end;

function TATSynEdit.GetMarkersArray: TATMarkerMarkerArray;
begin
  SetLength(Result{%H-}, 0);
  if Assigned(FMarkers) then
    Result:= FMarkers.AsMarkerArray;
end;

function TATSynEdit.GetAttribsArray: TATMarkerAttribArray;
begin
  SetLength(Result{%H-}, 0);
  if Assigned(FAttribs) then
    Result:= FAttribs.AsAttribArray;
end;

procedure TATSynEdit.SetCaretsArray(const Ar: TATPointArray);
begin
  if Assigned(FCarets) then
  begin
    FCarets.AsArray:= Ar;
    DoCaretsFixIncorrectPos(false);
  end;
end;

procedure TATSynEdit.SetMarkersArray(const Ar: TATMarkerMarkerArray);
begin
  InitMarkers; //FMarkers can be nil
  if Assigned(FMarkers) then
    FMarkers.AsMarkerArray:= Ar;
end;

procedure TATSynEdit.SetAttribsArray(const Ar: TATMarkerAttribArray);
begin
  InitAttribs; //FAttribs can be nil
  if Assigned(FAttribs) then
    FAttribs.AsAttribArray:= Ar;
end;

procedure TATSynEdit.DoCaretsExtend(ADown: boolean; ALines: integer);
var
  Caret: TATCaretItem;
  MoreCarets: TATCarets;
  St: TATStrings;
  NLen: integer;
  X, Y: integer;
  i, j: integer;
begin
  if Carets.Count=0 then exit;
  St:= Strings;

  //it's needed to Undo to multi-carets, not to single caret
  St.SetGroupMark;

  if not FCaretVirtual then
    case FLastCommand of
      cCommand_CaretsExtendUpLine,
      cCommand_CaretsExtendUpPage,
      cCommand_CaretsExtendUpToTop,
      cCommand_CaretsExtendDownLine,
      cCommand_CaretsExtendDownPage,
      cCommand_CaretsExtendDownToEnd:
        begin
        end
      else
        begin
          for i:= 0 to Carets.Count-1 do
          begin
            Caret:= Carets[i];
            Caret.BeforeExtendX:= Caret.PosX;
          end;
        end;
    end;

  MoreCarets:= TATCarets.Create;
  try
    for i:= 0 to Carets.Count-1 do
    begin
      Caret:= Carets[i];
      for j:= 1 to ALines do
      begin
        if not St.IsIndexValid(Caret.PosY) then Break;
        X:= St.CharPosToColumnPos(Caret.PosY, Caret.PosX, FTabHelper);
        Y:= Caret.PosY+BoolToPlusMinusOne[ADown]*j;
        if not St.IsIndexValid(Y) then Break;
        X:= St.ColumnPosToCharPos(Y, X, FTabHelper);

        //restore X from memory, if we made X smaller (like in Notepad++)
        if not FCaretVirtual then
        begin
          NLen:= St.LinesLen[Y];
          if X<NLen then
            if X<Caret.BeforeExtendX then
              X:= Caret.BeforeExtendX;
          X:= Min(X, NLen);
        end;

        MoreCarets.Add(X, Y);
        if not FCaretVirtual then
          MoreCarets[MoreCarets.Count-1].BeforeExtendX:= Caret.BeforeExtendX;
      end;
    end;

    for i:= 0 to MoreCarets.Count-1 do
    begin
      Caret:= MoreCarets[i];
      Carets.Add(Caret.PosX, Caret.PosY);
      if not FCaretVirtual then
        Carets[Carets.Count-1].BeforeExtendX:= Caret.BeforeExtendX;
    end;

    //must remove many dups
    Carets.Sort;
  finally
    FreeAndNil(MoreCarets);
  end;
end;

procedure TATSynEdit.DoCaretsAssign(NewCarets: TATCarets);
begin
  Carets.Clear;
  if NewCarets.Count>0 then
    Carets.Assign(NewCarets)
  else
    DoCaretSingle(0, 0);
end;


procedure TATSynEdit.DoCaretsFixIfInsideFolded;
var
  St: TATStrings;
  Caret: TATCaretItem;
  NFoldFrom, i: integer;
  bHidden: boolean;
begin
  St:= Strings;
  for i:= Carets.Count-1 downto 0 do
  begin
    Caret:= Carets[i];
    if Caret.EndY>=0 then Continue;
    if not St.IsIndexValid(Caret.PosY) then Continue;

    bHidden:= St.LinesHidden[Caret.PosY, FEditorIndex];
    if bHidden then
    begin
      Caret.PosX:= 0;
      Continue;
    end;

    NFoldFrom:= St.LinesFoldFrom[Caret.PosY, FEditorIndex];
    if NFoldFrom=0 then
      Continue
    else
      Caret.PosX:= Min(Caret.PosX, NFoldFrom-1);
  end;
end;


function TATSynEdit.DoCaretsFixIncorrectPos(AndLimitByLineEnds: boolean): boolean;
var
  St: TATStrings;
  Caret: TATCaretItem;
  NLast, NLen, i: integer;
begin
  Result:= false;
  St:= Strings;

  NLast:= St.Count-1;
  if NLast<0 then
  begin
    Result:= true;
    DoCaretSingle(0, 0);
    exit
  end;

  for i:= 0 to Carets.Count-1 do
  begin
    Caret:= Carets[i];

    //fix position
    if not St.IsIndexValid(Caret.PosY) then
    begin
      Result:= true;
      Caret.PosY:= NLast;
      Caret.EndX:= -1;
      Caret.EndY:= -1;
    end;

    if AndLimitByLineEnds or not FCaretVirtual then
    begin
      NLen:= St.LinesLen[Caret.PosY];
      if Caret.PosX>NLen then
      begin
        Result:= true;
        Caret.PosX:= NLen;
      end;
    end;

    //fix selection end
    if Caret.EndY>=0 then
    begin
      if not St.IsIndexValid(Caret.EndY) then
      begin
        Result:= true;
        Caret.EndY:= NLast;
      end;
      NLen:= St.LinesLen[Caret.EndY];
      if Caret.EndX>NLen then
      begin
        Result:= true;
        Caret.EndX:= NLen;
      end;
    end;
  end;
end;


procedure TATSynEdit.UpdateCaretsAndMarkersOnEditing(
  AFromCaret: integer; APos, APosEnd, AShift, APosAfter: TPoint);
//for text change at unknown caret, pass AFromCaret=0
var
  i: integer;
  {$ifdef debug_markers_shift}
  S: string;
  {$endif}
begin
  if APos.X<0 then Exit;
  if APos.Y<0 then Exit;

  //if AFromCaret<0 then
  //  AFromCaret:= Carets.IndexOfPosYAvg(APosY);

  if AFromCaret>=0 then
    for i:= AFromCaret to Carets.Count-1 do
      Carets[i].UpdateOnEditing(APos, APosEnd, AShift);

  {$ifdef debug_markers_shift}
  S:= '';
  for i:= 0 to Markers.Count-1 do
    with Markers[i] do
      S+= Format('Mark[%d] %d %d, ', [i, PosX, PosY]);
  Application.MainForm.Caption:= S+' -- '+Format(
    'Pos %d %d, Shift %d %d, PosAfter %d %d',
    [APos.X, APos.Y, AShift.X, AShift.Y, APosAfter.X, APosAfter.Y]);
  {$endif}

  //adjust markers
  if Assigned(FMarkers) then
    FMarkers.UpdateOnEditing(APos, APosEnd, AShift, APosAfter);

  if Assigned(FAttribs) then
    FAttribs.UpdateOnEditing(APos, APosEnd, AShift, APosAfter);

  if Assigned(FMarkedRange) then
    FMarkedRange.UpdateOnEditing(APos, APosEnd, AShift, APosAfter);
end;


procedure TATSynEdit.UpdateMarkersOnDeleting(AX1, AY1, AX2, AY2: integer);
begin
  if Assigned(FMarkers) then
    FMarkers.DeleteInRange(AX1, AY1, AX2, AY2);

  if Assigned(FAttribs) then
    FAttribs.DeleteInRange(AX1, AY1, AX2, AY2);

  if Assigned(FMarkedRange) then
    FMarkedRange.DeleteInRange(AX1, AY1, AX2, AY2);
end;

procedure TATSynEdit.DoCaretForceShow; inline;
begin
  FCaretAllowNextBlink:= true;
end;


function TATSynEdit.DoCaretsKeepOnScreen(AMoveDown: boolean): boolean;
//Forces carets to visible screen area. Gets true if some caret modified.
var
  Caret: TATCaretItem;
  Pnt: TPoint;
  Details: TATEditorPosDetails;
  NProximity, NRectTop, NRectBottom: integer;
  i: integer;
begin
  Result:= false;
  //"keep caret on screen" works bad for multi-carets
  if Carets.Count>1 then exit;

  NProximity:= GetActualProximityVert;
  NRectTop:= FRectMain.Top+NProximity*FCharSize.Y;
  NRectBottom:= Max(FRectMain.Top, FRectMain.Bottom-NProximity*FCharSize.Y);

  for i:= 0 to Carets.Count-1 do
  begin
    Caret:= Carets[i];
    if AMoveDown and (Caret.CoordY<=NRectTop+2) then
    begin
      Pnt:= Point(Caret.CoordX, NRectTop);
      Pnt:= ClientPosToCaretPos(Pnt, Details);
      Caret.PosX:= Pnt.X;
      Caret.PosY:= Pnt.Y;
      Result:= true;
    end
    else
    if not AMoveDown and (Caret.CoordY>=NRectBottom-2*FCharSize.Y-2) then
    begin
      Pnt:= Point(Caret.CoordX, NRectBottom-FCharSize.Y);
      Pnt:= ClientPosToCaretPos(Pnt, Details);
      Caret.PosX:= Pnt.X;
      Caret.PosY:= Pnt.Y;
      Result:= true;
    end;
  end;
end;


procedure TATSynEdit.DoCaretsApplyShape(var R: TRect; Props: TATCaretShape; W, H: integer);
var
  Y0, DY: integer;
begin
  //width>100% is not needed
  if Props.Width>=0 then
    R.Right:= R.Left + Props.Width
  else
    R.Right:= R.Left - W * Props.Width div 100;

  if Props.Height>=0 then
    R.Top:= R.Bottom - Props.Height
  else
  if Props.Height<-100 then
  begin
    Y0:= (R.Top+R.Bottom) div 2;
    DY:= -H * Props.Height div 200;
    R.Top:= Y0-DY;
    R.Bottom:= Y0+DY;
  end
  else
    R.Top:= R.Bottom + H * Props.Height div 100;
end;


function TATSynEdit.IsCaretOnVisibleRect: boolean;
begin
  //must refresh carets' CoordX/CoordY first, otherwise 1st carets' move out of
  //visible rect will not be detected
  UpdateCaretsCoords(false, true);

  Result:= Carets.IsAnyCaretInVisibleRect(FRectMain);
end;

function TATSynEdit.IsCaretFarFromVertEdge(ACommand: integer): boolean;
var
  NProximity, NPixels1, NPixels2: integer;
  Caret: TATCaretItem;
  bAllowMultiCarets, bMoveDown: boolean;
begin
  Result:= false;
  NProximity:= GetActualProximityVert;
  if NProximity=0 then exit;

  case Carets.Count of
    0:
      exit;
    1:
      Caret:= Carets[0];
    else
      begin
        IsCommandForMultiCarets(ACommand, bAllowMultiCarets, bMoveDown);
        if bAllowMultiCarets and bMoveDown then
          Caret:= Carets[Carets.Count-1]
        else
          Caret:= Carets[0];
      end;
  end;

  if Caret.CoordY=-1 then exit;

  NPixels1:= NProximity*FCharSize.Y;
  NPixels2:= (NProximity+1)*FCharSize.Y;
  Result:=
    (Abs(Caret.CoordY-FRectMain.Top)>=NPixels1) and
    (Abs(Caret.CoordY-FRectMain.Bottom)>=NPixels2);
end;

{ TATCaretShape }

function TATCaretShape.IsNarrow: boolean;
//negative values are in %, positive are in pixels
begin
  Result:=
    ((Width>=-25) and (Width<=2)) or
    ((Height>=-25) and (Height<=3));
end;

procedure TATCaretShape.Assign(Obj: TATCaretShape);
begin
  Width:= Obj.Width;
  Height:= Obj.Height;
  EmptyInside:= Obj.EmptyInside;
end;

