{$ifdef nn}begin end;{$endif}

function IsStreamWithSignature(Stream: TStream; const Sign: string): boolean;
const
  cMinimalCountOfChars = 1; //even empty text with only BOM must detect this BOM
var
  Buf: string;
begin
  Result:= false;
  if Stream.Size<Length(Sign)*cMinimalCountOfChars then Exit;
  Buf:= '';
  SetLength(Buf, Length(Sign));
  Stream.Position:= 0;
  Stream.ReadBuffer(Buf[1], Length(Sign));
  Stream.Position:= 0;
  Result:= Buf=Sign;
end;


function IsByteAscii(value: byte): boolean; inline;
begin
  case value of
    9, 10, 13,
    32..126:
      Result:= true;
    else
      Result:= false;
  end;
end;


function DetectStreamUtf16NoBom(Stream: TStream; BufSizeWords: integer; out IsLE: boolean): boolean;
var
  Buf: array[0..80] of word;
  BufItem: word;
  NSize, i: integer;
  CountLE, CountBE: integer;
  DetectLE, DetectBE: boolean;
begin
  Result:= false;
  if Stream.Size<6 then exit;

  if BufSizeWords<3 then BufSizeWords:= 3;
  if BufSizeWords>High(Buf) then BufSizeWords:= High(Buf);
  i:= Stream.Size div 2;
  if BufSizeWords>i then BufSizeWords:= i;

  NSize:= BufSizeWords*2;

  FillChar(Buf{%H-}, SizeOf(Buf), 0);
  Stream.Position:= 0;
  Stream.Read(Buf, NSize);
  Stream.Position:= 0;

  CountLE:= 0;
  CountBE:= 0;

  for i:= 0 to BufSizeWords-1 do
  begin
    BufItem:= Buf[i];
    if (Hi(BufItem)=0) and IsByteAscii(Lo(BufItem)) then
      Inc(CountLE)
    else
    if (Lo(BufItem)=0) and IsByteAscii(Hi(BufItem)) then
      Inc(CountBE);
  end;

  DetectLE:= (CountBE=0) and (CountLE>=BufSizeWords div 3 * 2);
  DetectBE:= (CountLE=0) and (CountBE>=BufSizeWords div 3 * 2);

  Result:= DetectLE or DetectBE;
  IsLE:= DetectLE;
end;


function DetectStreamUtf8NoBom(Stream: TStream; BufSizeKb: word): TBufferUTF8State;
const
  cMinLen = 10;
var
  Buf: PChar;
  Size: integer;
begin
  Result:= u8sUnknown;
  if Stream.Size<cMinLen then Exit;

  if BufSizeKb=0 then BufSizeKb:= 1;
  Size:= BufSizeKb*1024;
  GetMem(Buf, Size);
  try
    FillChar(Buf^, Size, 0);
    Stream.Position:= 0;
    Stream.Read(Buf^, Size-1{trail zero});
    Stream.Position:= 0;
    Result:= IsBufferUtf8(Buf, Size-1, true);
  finally
    FreeMem(Buf);
  end;
end;

function FixEncodingNameByIANAStandart(const S: string): string;
//correct some names according to this standard:
// IANA Registered Character Sets
// https://www.iana.org/assignments/character-sets/character-sets.xhtml
begin
  Result:= LowerCase(S);
  Result:= StringReplace(Result, 'windows-', 'cp', []);
  Result:= StringReplace(Result, 'ibm', 'cp', []);
  Result:= StringReplace(Result, 'iso_', 'iso-', []);
  Result:= StringReplace(Result, 'utf-8', 'utf8', []);
  if (Result='us') or (Result='us-ascii') then //this maps to cp367 but Lazarus don't have it
    Result:= 'cp437';
end;

function DetectStreamEncodingByXmlId(Stream: TStream; var AEnc: TATFileEncoding; var AEncCodepage: TEncConvId): boolean;
//detect in XML files by signature <?xml version="1.0" encoding="ISO-8859-9"?>
const
  cTagXML = '<?xml ';
  cTagEnc = ' encoding="';
var
  Buf: array[0..70] of char;
  TempEnc: TEncConvId;
  S: string;
  N: integer;
begin
  Result:= false;
  FillChar(Buf{%H-}, SizeOf(Buf), 0);
  Stream.Position:= 0;
  N:= Stream.Read(Buf, SizeOf(Buf)-1);
  Stream.Position:= 0;
  if N<=20 then exit;

  SetString(S, Buf, N);
  SDeleteFrom(S, #10);
  SDeleteFrom(S, #13);

  N:= Pos(cTagXML, S);
  if N=0 then exit;
  N:= Pos(cTagEnc, S);
  if N=0 then exit;
  Delete(S, 1, N-1+Length(cTagEnc));
  SDeleteFrom(S, '"');

  S:= FixEncodingNameByIANAStandart(S);

  TempEnc:= EncConvFindEncoding(S);
  case TempEnc of
    eidUTF8,
    eidUTF8BOM:
      begin
        AEnc:= cEncUTF8;
        Result:= true;
      end;
    eidUCS2LE,
    eidUCS2BE:
      exit;
    else
      begin
        AEnc:= cEncAnsi;
        AEncCodepage:= TempEnc;
        Result:= true;
      end;
  end;
end;

function DetectStreamEncodingByPythonId(Stream: TStream; var AEnc: TATFileEncoding; var AEncCodepage: TEncConvId): boolean;
{
https://docs.python.org/3/reference/lexical_analysis.html#encoding-declarations
The recommended forms of an encoding expression are

# -*- coding: <encoding-name> -*-

which is recognized also by GNU Emacs, and

# vim:fileencoding=<encoding-name>

which is recognized by VIM.
}
  //
  function IsLineComment(const S: string): boolean;
  begin
    Result:=
      SBeginsWith(S, '#') or
      SBeginsWith(S, ';') or
      SBeginsWith(S, '//') or
      SBeginsWith(S, '--');
  end;
  //
  function GetEnc(const S: string): string;
  var
    N, N0, Len: integer;
  begin
    Result:= '';
    N:= Pos('coding', S);
    if N=0 then exit;
    Len:= Length(S);
    if N+8>Len then exit;
    Inc(N, 6);
    if not (S[N] in ['=', ':']) then exit;
    Inc(N);
    while (N<=Len) and (S[N]=' ') do Inc(N);
    N0:= N;
    while (N<=Len) and (S[N] in ['a'..'z', 'A'..'Z', '0'..'9', '-']) do Inc(N);
    Result:= Copy(S, N0, N-N0);

    Result:= FixEncodingNameByIANAStandart(Result);
  end;
  //
const
  cBufSize = 100;
  cLineCount = 2; //detect by first 2 lines, 2nd used if 1st is comment
var
  Buf, SLine, SEncId: string;
  Sep: TATStringSeparator;
  i: integer;
begin
  Result:= false;
  Buf:= StringOfChar(' ', cBufSize);
  Stream.Position:= 0;
  i:= Stream.Read(Buf[1], Length(Buf));
  SetLength(Buf, i);
  Stream.Position:= 0;

  Buf:= StringReplace(Buf, #13#10, #10, [rfReplaceAll]);
  Buf:= StringReplace(Buf, #13, #10, [rfReplaceAll]);
  Sep.Init(Buf, #10);

  for i:= 0 to cLineCount-1 do
  begin
    Sep.GetItemStr(SLine);
    if SLine='' then exit;
    if not IsLineComment(SLine) then exit;
    SEncId:= LowerCase(GetEnc(SLine));
    if SEncId<>'' then
    begin
      AEncCodepage:= EncConvFindEncoding(SEncId);
      case AEncCodepage of
        eidUTF8,
        eidUTF8BOM,
        eidUCS2LE,
        eidUCS2BE:
          begin
            AEnc:= cEncUTF8;
            AEncCodepage:= eidUTF8;
          end
        else
          AEnc:= cEncAnsi;
      end;
      Result:= true;
      exit;
    end;
  end;
end;

procedure DetectStreamEncoding(Stream: TStream;
  var Enc: TATFileEncoding;
  var EncCodepage: TEncConvId;
  out SignLen: integer;
  out EncWithBom: boolean;
  BufSizeUTF8: integer;
  BufSizeUTF16: integer;
  DetectByPythonSignature: boolean;
  DetectByXmlSignature: boolean);
var
  IsLE: boolean;
begin
  SignLen:= 0;
  EncWithBom:= false;

  if IsStreamWithSignature(Stream, cSignUTF8) then
  begin
    Enc:= cEncUTF8;
    EncWithBom:= true;
    SignLen:= Length(cSignUTF8);
    Exit
  end;

  if IsStreamWithSignature(Stream, cSign32LE) then
  begin
    Enc:= cEnc32LE;
    EncWithBom:= true;
    SignLen:= Length(cSign32LE);
    Exit
  end;

  if IsStreamWithSignature(Stream, cSign32BE) then
  begin
    Enc:= cEnc32BE;
    EncWithBom:= true;
    SignLen:= Length(cSign32BE);
    Exit
  end;

  if IsStreamWithSignature(Stream, cSignWideLE) then
  begin
    Enc:= cEncWideLE;
    EncWithBom:= true;
    SignLen:= Length(cSignWideLE);
    Exit
  end;

  if IsStreamWithSignature(Stream, cSignWideBE) then
  begin
    Enc:= cEncWideBE;
    EncWithBom:= true;
    SignLen:= Length(cSignWideBE);
    Exit
  end;

  if BufSizeUTF8>0 then
    case DetectStreamUtf8NoBom(Stream, BufSizeUTF8) of
      u8sYes:
        begin
          Enc:= cEncUTF8;
          EncWithBom:= false;
          Exit
        end;
      u8sNo:
        begin
          // https://github.com/Alexey-T/CudaText/issues/2928
          Enc:= cEncAnsi;
        end;
    end;

  if DetectByPythonSignature then
    if DetectStreamEncodingByPythonId(Stream, Enc, EncCodepage) then
      Exit;

  if DetectByXmlSignature then
    if DetectStreamEncodingByXmlId(Stream, Enc, EncCodepage) then
      Exit;

  if BufSizeUTF16>0 then
    if DetectStreamUtf16NoBom(Stream, BufSizeUTF16, IsLE) then
    begin
      if IsLE then
        Enc:= cEncWideLE
      else
        Enc:= cEncWideBE;
      Exit
    end;
end;


procedure TATStrings.LoadFromStrings(AList: TStrings; AEnds: TATLineEnds);
var
  i: integer;
begin
  if ReadOnly then exit;
  Clear;
  if AList.Count=0 then
  begin
    ActionAddFakeLineIfNeeded;
    Exit;
  end;

  for i:= 0 to AList.Count-1 do
    LineAddRaw_NoUndo(AList[i], AEnds);

  ActionAddFakeLineIfNeeded;
  Modified:= true;
  for i:= 0 to Count-1 do
    SetLineState(i, cLineStateAdded);
end;

procedure TATStrings.LoadFromString(const AText: string);
var
  MS: TMemoryStream;
  Str: UnicodeString;
  EncOld: TATFileEncoding;
  i: integer;
begin
  if ReadOnly then exit;
  Clear;
  if AText='' then
  begin
    ActionAddFakeLineIfNeeded;
    Exit;
  end;

  if OneLine then
  begin
    Str:= UTF8Decode(AText);
    SDeleteFromEol(Str);
    LineAddEx(Str, cEndNone);
  end
  else
  begin
    MS:= TMemoryStream.Create;
    EncOld:= Encoding;
    try
      MS.Write(AText[1], Length(AText)*1);
      MS.Position:= 0;
      try
        Encoding:= cEncUTF8;
        EncodingDetect:= false;
        LoadFromStream(MS, true{AFromUTF8});

        //fixing CudaText issue #4172
        if not SEndsWithEol(AText) then
          ActionDeleteFakeLineAndFinalEol;
      finally
        Encoding:= EncOld;
        ActionAddFakeLineIfNeeded;
      end;
    finally
      FreeAndNil(MS);
    end;
  end;

  Modified:= true;
  for i:= 0 to Count-1 do
    SetLineState(i, cLineStateAdded);
end;

type
  TATCharEnding = (
    cCharNotEnding,
    cCharEnding10,
    cCharEnding13
    );

function _GetBufferEnding(ABuffer: pointer; APos: integer; AEnc: TATFileEncoding): TATCharEnding; inline;
var
  p: PByte absolute ABuffer;
  Check: byte;
begin
  Result:= cCharNotEnding;
  case AEnc of
    cEncAnsi,
    cEncUTF8:
      begin
        Check:= p[APos];
      end;
    cEncWideLE:
      begin
        if p[APos+1]<>0 then exit;
        Check:= p[APos];
      end;
    cEncWideBE:
      begin
        if p[APos]<>0 then exit;
        Check:= p[APos+1];
      end;
    cEnc32LE:
      begin
        if p[APos+3]<>0 then exit;
        if p[APos+2]<>0 then exit;
        if p[APos+1]<>0 then exit;
        Check:= p[APos];
      end;
    cEnc32BE:
      begin
        if p[APos]<>0 then exit;
        if p[APos+1]<>0 then exit;
        if p[APos+2]<>0 then exit;
        Check:= p[APos+3];
      end;
  end;

  case Check of
    10: Result:= cCharEnding10;
    13: Result:= cCharEnding13;
  end;
end;

procedure TATStrings.DoLoadFromStream(Stream: TStream;
  AFromUTF8: boolean; out AForcedToANSI: boolean);
var
  Buf: PAnsiChar;
  BufSize: int64;
  CharSize: integer;
  NSignatureLen: integer;
  NStreamSize: Int64;
  //------
  procedure _FindNextEol(AFromPos: integer;
    out AFoundPos: integer;
    out AEnding: TATLineEnds);
  begin
    AFoundPos:= AFromPos;
    AEnding:= cEndNone;
    repeat
      if AFoundPos>=BufSize then exit;

      if (AFoundPos>0) and (AFoundPos mod TATEditorOptions.ProgressLoadChars = 0) then
      begin
        FProgressValue:= Int64(AFoundPos)*100 div NStreamSize;
        if Assigned(FOnProgress) then
          FOnProgress(Self);
      end;

      case _GetBufferEnding(Buf, AFoundPos, FEncoding) of
        cCharNotEnding:
          begin
            Inc(AFoundPos, CharSize);
          end;
        cCharEnding10:
          begin
            AEnding:= cEndUnix;
            exit
          end;
        cCharEnding13:
          begin
            if AFoundPos+CharSize<BufSize then
              if _GetBufferEnding(Buf, AFoundPos+CharSize, FEncoding)=cCharEnding10 then
                AEnding:= cEndWin
              else
                AEnding:= cEndMac;
            exit
          end;
      end;
    until false;
  end;
  //------
  procedure _LoadItems;
  var
    Len, NStart, NEnd: integer;
    //NPrevProgressPos: integer;
    LineEnd: TATLineEnds;
    SA, SUtf8: string;
    SWide: UnicodeString;
    SQuad: UCS4String;
  begin
    NStart:= 0;
    //NPrevProgressPos:= 0;

    FProgressKind:= cStringsProgressLoading;
    FProgressValue:= 0;

    repeat
      _FindNextEol(NStart, NEnd, LineEnd);
      Len:= NEnd-NStart;

      {
      //call OnProgress each 1M chars
      //it's smooth enough progress, on 280Mb log, 10 seconds
      //2022.01: not needed now, because we call OnProgress in _FindNextEol

      if NStart-NPrevProgressPos>TATEditorOptions.ProgressLoadChars then
      begin
        NPrevProgressPos:= NStart;
        FProgressValue:= Int64(NStart)*100 div NStreamSize;
        if Assigned(FOnProgress) then
          FOnProgress(Self);
      end;
      }

      if LineEnd=cEndWin then
        Inc(NEnd, CharSize*2)
      else
        Inc(NEnd, CharSize);

      if Len=0 then
        LineAddRaw_NoUndo('', LineEnd)
      else
      begin
        case FEncoding of
          cEncAnsi:
            begin
              SetString(SA, @Buf[NStart], Len);
              SUtf8:= EncConvertToUTF8(SA, FEncodingCodepage);
              LineAddRaw_NoUndo(SUtf8, LineEnd);
            end;

          cEncUTF8:
            begin
              //this now raises exception if UTF8 content is bad, it's needed
              SetString(SA, @Buf[NStart], Len);
              LineAddRaw_NoUndo(SA, LineEnd);
            end;

          cEncWideLE,
          cEncWideBE:
            begin
              SWide:= '';
              SetLength(SWide, Len div 2);
              Move(Buf[NStart], SWide[1], Len);
              if FEncoding=cEncWideBE then
                SSwapEndianWide(SWide);
              LineAddRaw_NoUndo(SWide, LineEnd);
            end;

          cEnc32LE,
          cEnc32BE:
            begin
              SetLength(SQuad{%H-}, Len div 4+1);
              Move(Buf[NStart], SQuad[0], Len);
              if FEncoding=cEnc32BE then
                SSwapEndianUCS4(SQuad);
              SWide:= UCS4StringToUnicodeString(SQuad);
              LineAddRaw_NoUndo(SWide, LineEnd);
            end;

          else
            DoEncError;
        end;
      end;

      NStart:= NEnd;
      if (NStart>=BufSize) then Break;
    until false;

  end;
  //------
var
  bWithBom: boolean;
begin
  AForcedToANSI:= false;

  //LoadFromFile already calls Clear
  if Count>0 then
    Clear(false{AWithEvent});

  bWithBom:= false;
  NStreamSize:= Stream.Size;
  NSignatureLen:= 0;

  if AFromUTF8 then
  begin
    //func param forces UTF8
    FEncoding:= cEncUTF8;
  end
  else
  if NStreamSize>ATEditorOptions.MaxFileSizeMbToDetectEncoding*1024*1024 then
  begin
    //detect encoding by signature, even for huge files
    DetectStreamEncoding(Stream, FEncoding, FEncodingCodepage, NSignatureLen, bWithBom,
      0, //buffer size=0
      0, //buffer size=0
      ATEditorOptions.DetectEncodingByPythonSignature,
      ATEditorOptions.DetectEncodingByXmlSignature
      );
    //no signature?
    //it's safe to set here UTF8 even for binary files, later contents will be checked
    if not bWithBom then
      FEncoding:= cEncUTF8;
  end
  else
  if FEncodingDetect then
  begin
    //detect encoding by signature and by buffer content
    if FEncodingDetectDefaultUtf8 then
      FEncoding:= cEncUTF8
    else
      FEncoding:= cEncAnsi;
    DetectStreamEncoding(Stream, FEncoding, FEncodingCodepage, NSignatureLen, bWithBom,
      ATEditorOptions.DetectUTF8BufferKb,
      ATEditorOptions.DetectUTF16BufferWords,
      ATEditorOptions.DetectEncodingByPythonSignature,
      ATEditorOptions.DetectEncodingByXmlSignature
      );
  end
  else
  begin
    //detect encoding only by signature
    DetectStreamEncoding(Stream, FEncoding, FEncodingCodepage, NSignatureLen, bWithBom,
      0, //buffer sizes=0
      0,
      false,
      false
      );
  end;

  case FEncoding of
    cEncUTF8:
      SaveSignUtf8:= bWithBom;
    cEncWideBE,
    cEncWideLE:
      SaveSignWide:= bWithBom;
  end;

  CharSize:= cEncodingSize[FEncoding];

  BufSize:= NStreamSize-NSignatureLen;
  if BufSize<=0 then Exit;

  GetMem(Buf, BufSize);
  try
    Stream.Position:= NSignatureLen;
    Stream.ReadBuffer(Buf^, BufSize);

    try
      _LoadItems;
    except
      //failed to load text as UTF8, so load again as ANSI
      Clear; //clear loaded items
      AForcedToANSI:= true;
      FEncoding:= cEncAnsi;
      CharSize:= 1;
      _LoadItems;
    end;
  finally
    FreeMem(Buf);
    FProgressValue:= 0;
    FProgressKind:= cStringsProgressNone;
  end;
end;

procedure TATStrings.LoadFromStream(Stream: TStream; AFromUTF8: boolean);
begin
  ClearUndo(true);
  DoLoadFromStream(Stream, AFromUTF8, FLoadingForcedANSI);
  DoFinalizeLoading;
end;


procedure TATStrings.LoadFromFile(const AFilename: string);
var
  fs: TFileStream;
  ms: TMemoryStream;
begin
  fs:= TFileStream.Create(AFilename, fmOpenRead or fmShareDenyNone);
  try
    if fs.Size>=0 then
      LoadFromStream(fs)
    else
    begin
      ms:= TMemoryStream.Create;
      try
        _ReadFileToStream(ms, AFilename);
        LoadFromStream(ms);
      finally
        FreeAndNil(ms);
      end;
    end;
  finally
    FreeAndNil(fs);
  end;
end;

procedure TATStrings.DoFinalizeLoading;
begin
  DoDetectEndings;
  ActionAddFakeLineIfNeeded;
  ClearLineStates(false);
  ClearUndo;
  Modified:= false;
  FProgressValue:= 0;
end;


