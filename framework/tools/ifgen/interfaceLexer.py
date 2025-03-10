# $ANTLR 3.5.3 interface.g 2025-03-06 23:43:56

import sys
from antlr3 import *
from antlr3.compat import set, frozenset



# for convenience in actions
HIDDEN = BaseRecognizer.HIDDEN

# token types
EOF=-1
T__30=30
T__31=31
T__32=32
T__33=33
T__34=34
T__35=35
T__36=36
T__37=37
T__38=38
T__39=39
T__40=40
T__41=41
T__42=42
T__43=43
T__44=44
ALPHA=4
ALPHANUM=5
BITMASK=6
CPP_COMMENT=7
C_COMMENT=8
DEC_NUMBER=9
DEFINE=10
DOC_POST_COMMENT=11
DOC_PRE_COMMENT=12
ENUM=13
EVENT=14
FUNCTION=15
HANDLER=16
HEXNUM=17
HEX_NUMBER=18
IDENTIFIER=19
IN=20
NUM=21
OUT=22
QUOTED_STRING=23
REFERENCE=24
SCOPED_IDENTIFIER=25
SEMICOLON=26
STRUCT=27
USETYPES=28
WS=29


class interfaceLexer(Lexer):

    grammarFileName = "interface.g"
    api_version = 1

    def __init__(self, input=None, state=None):
        if state is None:
            state = RecognizerSharedState()
        super(interfaceLexer, self).__init__(input, state)

        self.delegates = []

        self.dfa13 = self.DFA13(
            self, 13,
            eot = self.DFA13_eot,
            eof = self.DFA13_eof,
            min = self.DFA13_min,
            max = self.DFA13_max,
            accept = self.DFA13_accept,
            special = self.DFA13_special,
            transition = self.DFA13_transition
            )

        self.dfa16 = self.DFA16(
            self, 16,
            eot = self.DFA16_eot,
            eof = self.DFA16_eof,
            min = self.DFA16_min,
            max = self.DFA16_max,
            accept = self.DFA16_accept,
            special = self.DFA16_special,
            transition = self.DFA16_transition
            )




             
    def getErrorHeader(self, e):
        """
        What is the error header, normally line/character position information?
        """

        source_name = self.getSourceName()
        if source_name is not None:
            return "{}:{}:{}: error:".format(source_name, e.line, e.charPositionInLine)
        return "{}:{}: error:".format(e.line, e.charPositionInLine)

    def getErrorMessage(self, e, tokenNames):
        """Give more user-friendly error messages for some lexing errors"""
        if isinstance(e, NoViableAltException):
            return "Unexpected character " + self.getCharErrorDisplay(e.c)
        else:
            return super(interfaceLexer, self).getErrorMessage(e, tokenNames)




    # $ANTLR start "T__30"
    def mT__30(self, ):
        try:
            _type = T__30
            _channel = DEFAULT_CHANNEL

            # interface.g:27:7: ( '(' )
            # interface.g:27:9: '('
            pass 
            self.match(40)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__30"



    # $ANTLR start "T__31"
    def mT__31(self, ):
        try:
            _type = T__31
            _channel = DEFAULT_CHANNEL

            # interface.g:28:7: ( ')' )
            # interface.g:28:9: ')'
            pass 
            self.match(41)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__31"



    # $ANTLR start "T__32"
    def mT__32(self, ):
        try:
            _type = T__32
            _channel = DEFAULT_CHANNEL

            # interface.g:29:7: ( '*' )
            # interface.g:29:9: '*'
            pass 
            self.match(42)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__32"



    # $ANTLR start "T__33"
    def mT__33(self, ):
        try:
            _type = T__33
            _channel = DEFAULT_CHANNEL

            # interface.g:30:7: ( '+' )
            # interface.g:30:9: '+'
            pass 
            self.match(43)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__33"



    # $ANTLR start "T__34"
    def mT__34(self, ):
        try:
            _type = T__34
            _channel = DEFAULT_CHANNEL

            # interface.g:31:7: ( ',' )
            # interface.g:31:9: ','
            pass 
            self.match(44)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__34"



    # $ANTLR start "T__35"
    def mT__35(self, ):
        try:
            _type = T__35
            _channel = DEFAULT_CHANNEL

            # interface.g:32:7: ( '-' )
            # interface.g:32:9: '-'
            pass 
            self.match(45)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__35"



    # $ANTLR start "T__36"
    def mT__36(self, ):
        try:
            _type = T__36
            _channel = DEFAULT_CHANNEL

            # interface.g:33:7: ( '..' )
            # interface.g:33:9: '..'
            pass 
            self.match("..")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__36"



    # $ANTLR start "T__37"
    def mT__37(self, ):
        try:
            _type = T__37
            _channel = DEFAULT_CHANNEL

            # interface.g:34:7: ( '/' )
            # interface.g:34:9: '/'
            pass 
            self.match(47)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__37"



    # $ANTLR start "T__38"
    def mT__38(self, ):
        try:
            _type = T__38
            _channel = DEFAULT_CHANNEL

            # interface.g:35:7: ( '=' )
            # interface.g:35:9: '='
            pass 
            self.match(61)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__38"



    # $ANTLR start "T__39"
    def mT__39(self, ):
        try:
            _type = T__39
            _channel = DEFAULT_CHANNEL

            # interface.g:36:7: ( 'API_VERSION' )
            # interface.g:36:9: 'API_VERSION'
            pass 
            self.match("API_VERSION")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__39"



    # $ANTLR start "T__40"
    def mT__40(self, ):
        try:
            _type = T__40
            _channel = DEFAULT_CHANNEL

            # interface.g:37:7: ( 'SERVING_VERSION' )
            # interface.g:37:9: 'SERVING_VERSION'
            pass 
            self.match("SERVING_VERSION")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__40"



    # $ANTLR start "T__41"
    def mT__41(self, ):
        try:
            _type = T__41
            _channel = DEFAULT_CHANNEL

            # interface.g:38:7: ( '[' )
            # interface.g:38:9: '['
            pass 
            self.match(91)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__41"



    # $ANTLR start "T__42"
    def mT__42(self, ):
        try:
            _type = T__42
            _channel = DEFAULT_CHANNEL

            # interface.g:39:7: ( ']' )
            # interface.g:39:9: ']'
            pass 
            self.match(93)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__42"



    # $ANTLR start "T__43"
    def mT__43(self, ):
        try:
            _type = T__43
            _channel = DEFAULT_CHANNEL

            # interface.g:40:7: ( '{' )
            # interface.g:40:9: '{'
            pass 
            self.match(123)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__43"



    # $ANTLR start "T__44"
    def mT__44(self, ):
        try:
            _type = T__44
            _channel = DEFAULT_CHANNEL

            # interface.g:41:7: ( '}' )
            # interface.g:41:9: '}'
            pass 
            self.match(125)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "T__44"



    # $ANTLR start "ALPHA"
    def mALPHA(self, ):
        try:
            # interface.g:182:16: ( 'a' .. 'z' | 'A' .. 'Z' )
            # interface.g:
            pass 
            if (65 <= self.input.LA(1) <= 90) or (97 <= self.input.LA(1) <= 122):
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse






        finally:
            pass

    # $ANTLR end "ALPHA"



    # $ANTLR start "NUM"
    def mNUM(self, ):
        try:
            # interface.g:183:14: ( '0' .. '9' )
            # interface.g:
            pass 
            if (48 <= self.input.LA(1) <= 57):
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse






        finally:
            pass

    # $ANTLR end "NUM"



    # $ANTLR start "HEXNUM"
    def mHEXNUM(self, ):
        try:
            # interface.g:184:17: ( NUM | 'a' .. 'f' | 'A' .. 'F' )
            # interface.g:
            pass 
            if (48 <= self.input.LA(1) <= 57) or (65 <= self.input.LA(1) <= 70) or (97 <= self.input.LA(1) <= 102):
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse






        finally:
            pass

    # $ANTLR end "HEXNUM"



    # $ANTLR start "ALPHANUM"
    def mALPHANUM(self, ):
        try:
            # interface.g:185:19: ( ALPHA | NUM )
            # interface.g:
            pass 
            if (48 <= self.input.LA(1) <= 57) or (65 <= self.input.LA(1) <= 90) or (97 <= self.input.LA(1) <= 122):
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse






        finally:
            pass

    # $ANTLR end "ALPHANUM"



    # $ANTLR start "SEMICOLON"
    def mSEMICOLON(self, ):
        try:
            _type = SEMICOLON
            _channel = DEFAULT_CHANNEL

            # interface.g:191:11: ( ';' )
            # interface.g:191:13: ';'
            pass 
            self.match(59)



            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "SEMICOLON"



    # $ANTLR start "IN"
    def mIN(self, ):
        try:
            _type = IN
            _channel = DEFAULT_CHANNEL

            # interface.g:194:4: ( 'IN' )
            # interface.g:194:6: 'IN'
            pass 
            self.match("IN")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "IN"



    # $ANTLR start "OUT"
    def mOUT(self, ):
        try:
            _type = OUT
            _channel = DEFAULT_CHANNEL

            # interface.g:195:5: ( 'OUT' )
            # interface.g:195:7: 'OUT'
            pass 
            self.match("OUT")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "OUT"



    # $ANTLR start "FUNCTION"
    def mFUNCTION(self, ):
        try:
            _type = FUNCTION
            _channel = DEFAULT_CHANNEL

            # interface.g:196:10: ( 'FUNCTION' )
            # interface.g:196:12: 'FUNCTION'
            pass 
            self.match("FUNCTION")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "FUNCTION"



    # $ANTLR start "HANDLER"
    def mHANDLER(self, ):
        try:
            _type = HANDLER
            _channel = DEFAULT_CHANNEL

            # interface.g:197:9: ( 'HANDLER' )
            # interface.g:197:11: 'HANDLER'
            pass 
            self.match("HANDLER")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "HANDLER"



    # $ANTLR start "EVENT"
    def mEVENT(self, ):
        try:
            _type = EVENT
            _channel = DEFAULT_CHANNEL

            # interface.g:198:7: ( 'EVENT' )
            # interface.g:198:9: 'EVENT'
            pass 
            self.match("EVENT")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "EVENT"



    # $ANTLR start "REFERENCE"
    def mREFERENCE(self, ):
        try:
            _type = REFERENCE
            _channel = DEFAULT_CHANNEL

            # interface.g:199:11: ( 'REFERENCE' )
            # interface.g:199:13: 'REFERENCE'
            pass 
            self.match("REFERENCE")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "REFERENCE"



    # $ANTLR start "DEFINE"
    def mDEFINE(self, ):
        try:
            _type = DEFINE
            _channel = DEFAULT_CHANNEL

            # interface.g:200:8: ( 'DEFINE' )
            # interface.g:200:10: 'DEFINE'
            pass 
            self.match("DEFINE")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "DEFINE"



    # $ANTLR start "ENUM"
    def mENUM(self, ):
        try:
            _type = ENUM
            _channel = DEFAULT_CHANNEL

            # interface.g:201:6: ( 'ENUM' )
            # interface.g:201:8: 'ENUM'
            pass 
            self.match("ENUM")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "ENUM"



    # $ANTLR start "BITMASK"
    def mBITMASK(self, ):
        try:
            _type = BITMASK
            _channel = DEFAULT_CHANNEL

            # interface.g:202:9: ( 'BITMASK' )
            # interface.g:202:11: 'BITMASK'
            pass 
            self.match("BITMASK")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "BITMASK"



    # $ANTLR start "STRUCT"
    def mSTRUCT(self, ):
        try:
            _type = STRUCT
            _channel = DEFAULT_CHANNEL

            # interface.g:203:8: ( 'STRUCT' )
            # interface.g:203:10: 'STRUCT'
            pass 
            self.match("STRUCT")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "STRUCT"



    # $ANTLR start "USETYPES"
    def mUSETYPES(self, ):
        try:
            _type = USETYPES
            _channel = DEFAULT_CHANNEL

            # interface.g:204:10: ( 'USETYPES' )
            # interface.g:204:12: 'USETYPES'
            pass 
            self.match("USETYPES")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "USETYPES"



    # $ANTLR start "IDENTIFIER"
    def mIDENTIFIER(self, ):
        try:
            _type = IDENTIFIER
            _channel = DEFAULT_CHANNEL

            # interface.g:209:12: ( ALPHA ( ALPHANUM | '_' )* )
            # interface.g:209:14: ALPHA ( ALPHANUM | '_' )*
            pass 
            self.mALPHA()


            # interface.g:209:20: ( ALPHANUM | '_' )*
            while True: #loop1
                alt1 = 2
                LA1_0 = self.input.LA(1)

                if ((48 <= LA1_0 <= 57) or (65 <= LA1_0 <= 90) or LA1_0 == 95 or (97 <= LA1_0 <= 122)) :
                    alt1 = 1


                if alt1 == 1:
                    # interface.g:
                    pass 
                    if (48 <= self.input.LA(1) <= 57) or (65 <= self.input.LA(1) <= 90) or self.input.LA(1) == 95 or (97 <= self.input.LA(1) <= 122):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    break #loop1




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "IDENTIFIER"



    # $ANTLR start "SCOPED_IDENTIFIER"
    def mSCOPED_IDENTIFIER(self, ):
        try:
            _type = SCOPED_IDENTIFIER
            _channel = DEFAULT_CHANNEL

            # interface.g:212:19: ( IDENTIFIER '.' IDENTIFIER )
            # interface.g:212:21: IDENTIFIER '.' IDENTIFIER
            pass 
            self.mIDENTIFIER()


            self.match(46)

            self.mIDENTIFIER()




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "SCOPED_IDENTIFIER"



    # $ANTLR start "DEC_NUMBER"
    def mDEC_NUMBER(self, ):
        try:
            _type = DEC_NUMBER
            _channel = DEFAULT_CHANNEL

            number = None

            # interface.g:215:12: (number= ( '0' | '1' .. '9' ( NUM )* ) )
            # interface.g:215:14: number= ( '0' | '1' .. '9' ( NUM )* )
            pass 
            # interface.g:215:21: ( '0' | '1' .. '9' ( NUM )* )
            alt3 = 2
            LA3_0 = self.input.LA(1)

            if (LA3_0 == 48) :
                alt3 = 1
            elif ((49 <= LA3_0 <= 57)) :
                alt3 = 2
            else:
                nvae = NoViableAltException("", 3, 0, self.input)

                raise nvae


            if alt3 == 1:
                # interface.g:215:23: '0'
                pass 
                number = self.input.LA(1)

                self.match(48)


            elif alt3 == 2:
                # interface.g:215:29: '1' .. '9' ( NUM )*
                pass 
                number = self.input.LA(1)

                self.matchRange(49, 57)

                # interface.g:215:38: ( NUM )*
                while True: #loop2
                    alt2 = 2
                    LA2_0 = self.input.LA(1)

                    if ((48 <= LA2_0 <= 57)) :
                        alt2 = 1


                    if alt2 == 1:
                        # interface.g:
                        pass 
                        if (48 <= self.input.LA(1) <= 57):
                            self.input.consume()
                        else:
                            mse = MismatchedSetException(None, self.input)
                            self.recover(mse)
                            raise mse




                    else:
                        break #loop2







            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "DEC_NUMBER"



    # $ANTLR start "HEX_NUMBER"
    def mHEX_NUMBER(self, ):
        try:
            _type = HEX_NUMBER
            _channel = DEFAULT_CHANNEL

            # interface.g:215:12: ( '0' ( 'x' | 'X' ) ( HEXNUM )+ )
            # interface.g:215:14: '0' ( 'x' | 'X' ) ( HEXNUM )+
            pass 
            self.match(48)

            if self.input.LA(1) == 88 or self.input.LA(1) == 120:
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse



            # interface.g:215:31: ( HEXNUM )+
            cnt4 = 0
            while True: #loop4
                alt4 = 2
                LA4_0 = self.input.LA(1)

                if ((48 <= LA4_0 <= 57) or (65 <= LA4_0 <= 70) or (97 <= LA4_0 <= 102)) :
                    alt4 = 1


                if alt4 == 1:
                    # interface.g:
                    pass 
                    if (48 <= self.input.LA(1) <= 57) or (65 <= self.input.LA(1) <= 70) or (97 <= self.input.LA(1) <= 102):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    if cnt4 >= 1:
                        break #loop4

                    eee = EarlyExitException(4, self.input)
                    raise eee

                cnt4 += 1




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "HEX_NUMBER"



    # $ANTLR start "QUOTED_STRING"
    def mQUOTED_STRING(self, ):
        try:
            _type = QUOTED_STRING
            _channel = DEFAULT_CHANNEL

            # interface.g:220:15: ( '\"' (~ ( '\\\"' | '\\\\' ) | '\\\\' . )* '\"' | '\\'' (~ ( '\\'' | '\\\\' ) | '\\\\' . )* '\\'' )
            alt7 = 2
            LA7_0 = self.input.LA(1)

            if (LA7_0 == 34) :
                alt7 = 1
            elif (LA7_0 == 39) :
                alt7 = 2
            else:
                nvae = NoViableAltException("", 7, 0, self.input)

                raise nvae


            if alt7 == 1:
                # interface.g:220:17: '\"' (~ ( '\\\"' | '\\\\' ) | '\\\\' . )* '\"'
                pass 
                self.match(34)

                # interface.g:220:21: (~ ( '\\\"' | '\\\\' ) | '\\\\' . )*
                while True: #loop5
                    alt5 = 3
                    LA5_0 = self.input.LA(1)

                    if ((0 <= LA5_0 <= 33) or (35 <= LA5_0 <= 91) or (93 <= LA5_0 <= 65535)) :
                        alt5 = 1
                    elif (LA5_0 == 92) :
                        alt5 = 2


                    if alt5 == 1:
                        # interface.g:220:23: ~ ( '\\\"' | '\\\\' )
                        pass 
                        if (0 <= self.input.LA(1) <= 33) or (35 <= self.input.LA(1) <= 91) or (93 <= self.input.LA(1) <= 65535):
                            self.input.consume()
                        else:
                            mse = MismatchedSetException(None, self.input)
                            self.recover(mse)
                            raise mse




                    elif alt5 == 2:
                        # interface.g:220:40: '\\\\' .
                        pass 
                        self.match(92)

                        self.matchAny()


                    else:
                        break #loop5


                self.match(34)


            elif alt7 == 2:
                # interface.g:221:7: '\\'' (~ ( '\\'' | '\\\\' ) | '\\\\' . )* '\\''
                pass 
                self.match(39)

                # interface.g:221:12: (~ ( '\\'' | '\\\\' ) | '\\\\' . )*
                while True: #loop6
                    alt6 = 3
                    LA6_0 = self.input.LA(1)

                    if ((0 <= LA6_0 <= 38) or (40 <= LA6_0 <= 91) or (93 <= LA6_0 <= 65535)) :
                        alt6 = 1
                    elif (LA6_0 == 92) :
                        alt6 = 2


                    if alt6 == 1:
                        # interface.g:221:14: ~ ( '\\'' | '\\\\' )
                        pass 
                        if (0 <= self.input.LA(1) <= 38) or (40 <= self.input.LA(1) <= 91) or (93 <= self.input.LA(1) <= 65535):
                            self.input.consume()
                        else:
                            mse = MismatchedSetException(None, self.input)
                            self.recover(mse)
                            raise mse




                    elif alt6 == 2:
                        # interface.g:221:31: '\\\\' .
                        pass 
                        self.match(92)

                        self.matchAny()


                    else:
                        break #loop6


                self.match(39)


            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "QUOTED_STRING"



    # $ANTLR start "DOC_PRE_COMMENT"
    def mDOC_PRE_COMMENT(self, ):
        try:
            _type = DOC_PRE_COMMENT
            _channel = DEFAULT_CHANNEL

            # interface.g:225:17: ( '/**' ~ '*' (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+ '*/' )
            # interface.g:225:19: '/**' ~ '*' (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+ '*/'
            pass 
            self.match("/**")


            if (0 <= self.input.LA(1) <= 41) or (43 <= self.input.LA(1) <= 65535):
                self.input.consume()
            else:
                mse = MismatchedSetException(None, self.input)
                self.recover(mse)
                raise mse



            # interface.g:225:30: (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+
            cnt9 = 0
            while True: #loop9
                alt9 = 3
                LA9_0 = self.input.LA(1)

                if (LA9_0 == 42) :
                    LA9_1 = self.input.LA(2)

                    if ((0 <= LA9_1 <= 46) or (48 <= LA9_1 <= 65535)) :
                        alt9 = 2


                elif ((0 <= LA9_0 <= 41) or (43 <= LA9_0 <= 65535)) :
                    alt9 = 1


                if alt9 == 1:
                    # interface.g:225:31: ~ '*'
                    pass 
                    if (0 <= self.input.LA(1) <= 41) or (43 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                elif alt9 == 2:
                    # interface.g:225:38: ( '*' )+ ~ ( '/' | '*' )
                    pass 
                    # interface.g:225:38: ( '*' )+
                    cnt8 = 0
                    while True: #loop8
                        alt8 = 2
                        LA8_0 = self.input.LA(1)

                        if (LA8_0 == 42) :
                            alt8 = 1


                        if alt8 == 1:
                            # interface.g:225:38: '*'
                            pass 
                            self.match(42)


                        else:
                            if cnt8 >= 1:
                                break #loop8

                            eee = EarlyExitException(8, self.input)
                            raise eee

                        cnt8 += 1


                    if (0 <= self.input.LA(1) <= 41) or (43 <= self.input.LA(1) <= 46) or (48 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    if cnt9 >= 1:
                        break #loop9

                    eee = EarlyExitException(9, self.input)
                    raise eee

                cnt9 += 1


            self.match("*/")




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "DOC_PRE_COMMENT"



    # $ANTLR start "DOC_POST_COMMENT"
    def mDOC_POST_COMMENT(self, ):
        try:
            _type = DOC_POST_COMMENT
            _channel = DEFAULT_CHANNEL

            # interface.g:228:18: ( '///<' (~ '\\n' )* )
            # interface.g:228:20: '///<' (~ '\\n' )*
            pass 
            self.match("///<")


            # interface.g:228:27: (~ '\\n' )*
            while True: #loop10
                alt10 = 2
                LA10_0 = self.input.LA(1)

                if ((0 <= LA10_0 <= 9) or (11 <= LA10_0 <= 65535)) :
                    alt10 = 1


                if alt10 == 1:
                    # interface.g:
                    pass 
                    if (0 <= self.input.LA(1) <= 9) or (11 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    break #loop10




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "DOC_POST_COMMENT"



    # $ANTLR start "WS"
    def mWS(self, ):
        try:
            _type = WS
            _channel = DEFAULT_CHANNEL

            # interface.g:231:4: ( ( ' ' | '\\t' | '\\n' )+ )
            # interface.g:231:6: ( ' ' | '\\t' | '\\n' )+
            pass 
            # interface.g:231:6: ( ' ' | '\\t' | '\\n' )+
            cnt11 = 0
            while True: #loop11
                alt11 = 2
                LA11_0 = self.input.LA(1)

                if ((9 <= LA11_0 <= 10) or LA11_0 == 32) :
                    alt11 = 1


                if alt11 == 1:
                    # interface.g:
                    pass 
                    if (9 <= self.input.LA(1) <= 10) or self.input.LA(1) == 32:
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    if cnt11 >= 1:
                        break #loop11

                    eee = EarlyExitException(11, self.input)
                    raise eee

                cnt11 += 1


            #action start
            self.skip() 
            #action end




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "WS"



    # $ANTLR start "C_COMMENT"
    def mC_COMMENT(self, ):
        try:
            _type = C_COMMENT
            _channel = DEFAULT_CHANNEL

            # interface.g:234:11: ( '/*' (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+ ( '*' )+ '/' )
            # interface.g:234:13: '/*' (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+ ( '*' )+ '/'
            pass 
            self.match("/*")


            # interface.g:234:18: (~ '*' | ( '*' )+ ~ ( '/' | '*' ) )+
            cnt13 = 0
            while True: #loop13
                alt13 = 3
                alt13 = self.dfa13.predict(self.input)
                if alt13 == 1:
                    # interface.g:234:19: ~ '*'
                    pass 
                    if (0 <= self.input.LA(1) <= 41) or (43 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                elif alt13 == 2:
                    # interface.g:234:26: ( '*' )+ ~ ( '/' | '*' )
                    pass 
                    # interface.g:234:26: ( '*' )+
                    cnt12 = 0
                    while True: #loop12
                        alt12 = 2
                        LA12_0 = self.input.LA(1)

                        if (LA12_0 == 42) :
                            alt12 = 1


                        if alt12 == 1:
                            # interface.g:234:26: '*'
                            pass 
                            self.match(42)


                        else:
                            if cnt12 >= 1:
                                break #loop12

                            eee = EarlyExitException(12, self.input)
                            raise eee

                        cnt12 += 1


                    if (0 <= self.input.LA(1) <= 41) or (43 <= self.input.LA(1) <= 46) or (48 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    if cnt13 >= 1:
                        break #loop13

                    eee = EarlyExitException(13, self.input)
                    raise eee

                cnt13 += 1


            # interface.g:234:47: ( '*' )+
            cnt14 = 0
            while True: #loop14
                alt14 = 2
                LA14_0 = self.input.LA(1)

                if (LA14_0 == 42) :
                    alt14 = 1


                if alt14 == 1:
                    # interface.g:234:47: '*'
                    pass 
                    self.match(42)


                else:
                    if cnt14 >= 1:
                        break #loop14

                    eee = EarlyExitException(14, self.input)
                    raise eee

                cnt14 += 1


            self.match(47)

            #action start
            self.skip() 
            #action end




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "C_COMMENT"



    # $ANTLR start "CPP_COMMENT"
    def mCPP_COMMENT(self, ):
        try:
            _type = CPP_COMMENT
            _channel = DEFAULT_CHANNEL

            # interface.g:237:13: ( '//' (~ '\\n' )* )
            # interface.g:237:15: '//' (~ '\\n' )*
            pass 
            self.match("//")


            # interface.g:237:20: (~ '\\n' )*
            while True: #loop15
                alt15 = 2
                LA15_0 = self.input.LA(1)

                if ((0 <= LA15_0 <= 9) or (11 <= LA15_0 <= 65535)) :
                    alt15 = 1


                if alt15 == 1:
                    # interface.g:
                    pass 
                    if (0 <= self.input.LA(1) <= 9) or (11 <= self.input.LA(1) <= 65535):
                        self.input.consume()
                    else:
                        mse = MismatchedSetException(None, self.input)
                        self.recover(mse)
                        raise mse




                else:
                    break #loop15


            #action start
            self.skip() 
            #action end




            self._state.type = _type
            self._state.channel = _channel
        finally:
            pass

    # $ANTLR end "CPP_COMMENT"



    def mTokens(self):
        # interface.g:1:8: ( T__30 | T__31 | T__32 | T__33 | T__34 | T__35 | T__36 | T__37 | T__38 | T__39 | T__40 | T__41 | T__42 | T__43 | T__44 | SEMICOLON | IN | OUT | FUNCTION | HANDLER | EVENT | REFERENCE | DEFINE | ENUM | BITMASK | STRUCT | USETYPES | IDENTIFIER | SCOPED_IDENTIFIER | DEC_NUMBER | HEX_NUMBER | QUOTED_STRING | DOC_PRE_COMMENT | DOC_POST_COMMENT | WS | C_COMMENT | CPP_COMMENT )
        alt16 = 37
        alt16 = self.dfa16.predict(self.input)
        if alt16 == 1:
            # interface.g:1:10: T__30
            pass 
            self.mT__30()



        elif alt16 == 2:
            # interface.g:1:16: T__31
            pass 
            self.mT__31()



        elif alt16 == 3:
            # interface.g:1:22: T__32
            pass 
            self.mT__32()



        elif alt16 == 4:
            # interface.g:1:28: T__33
            pass 
            self.mT__33()



        elif alt16 == 5:
            # interface.g:1:34: T__34
            pass 
            self.mT__34()



        elif alt16 == 6:
            # interface.g:1:40: T__35
            pass 
            self.mT__35()



        elif alt16 == 7:
            # interface.g:1:46: T__36
            pass 
            self.mT__36()



        elif alt16 == 8:
            # interface.g:1:52: T__37
            pass 
            self.mT__37()



        elif alt16 == 9:
            # interface.g:1:58: T__38
            pass 
            self.mT__38()



        elif alt16 == 10:
            # interface.g:1:64: T__39
            pass 
            self.mT__39()



        elif alt16 == 11:
            # interface.g:1:70: T__40
            pass 
            self.mT__40()



        elif alt16 == 12:
            # interface.g:1:76: T__41
            pass 
            self.mT__41()



        elif alt16 == 13:
            # interface.g:1:82: T__42
            pass 
            self.mT__42()



        elif alt16 == 14:
            # interface.g:1:88: T__43
            pass 
            self.mT__43()



        elif alt16 == 15:
            # interface.g:1:94: T__44
            pass 
            self.mT__44()



        elif alt16 == 16:
            # interface.g:1:100: SEMICOLON
            pass 
            self.mSEMICOLON()



        elif alt16 == 17:
            # interface.g:1:110: IN
            pass 
            self.mIN()



        elif alt16 == 18:
            # interface.g:1:113: OUT
            pass 
            self.mOUT()



        elif alt16 == 19:
            # interface.g:1:117: FUNCTION
            pass 
            self.mFUNCTION()



        elif alt16 == 20:
            # interface.g:1:126: HANDLER
            pass 
            self.mHANDLER()



        elif alt16 == 21:
            # interface.g:1:134: EVENT
            pass 
            self.mEVENT()



        elif alt16 == 22:
            # interface.g:1:140: REFERENCE
            pass 
            self.mREFERENCE()



        elif alt16 == 23:
            # interface.g:1:150: DEFINE
            pass 
            self.mDEFINE()



        elif alt16 == 24:
            # interface.g:1:157: ENUM
            pass 
            self.mENUM()



        elif alt16 == 25:
            # interface.g:1:162: BITMASK
            pass 
            self.mBITMASK()



        elif alt16 == 26:
            # interface.g:1:170: STRUCT
            pass 
            self.mSTRUCT()



        elif alt16 == 27:
            # interface.g:1:177: USETYPES
            pass 
            self.mUSETYPES()



        elif alt16 == 28:
            # interface.g:1:186: IDENTIFIER
            pass 
            self.mIDENTIFIER()



        elif alt16 == 29:
            # interface.g:1:197: SCOPED_IDENTIFIER
            pass 
            self.mSCOPED_IDENTIFIER()



        elif alt16 == 30:
            # interface.g:1:215: DEC_NUMBER
            pass 
            self.mDEC_NUMBER()



        elif alt16 == 31:
            # interface.g:1:226: HEX_NUMBER
            pass 
            self.mHEX_NUMBER()



        elif alt16 == 32:
            # interface.g:1:237: QUOTED_STRING
            pass 
            self.mQUOTED_STRING()



        elif alt16 == 33:
            # interface.g:1:251: DOC_PRE_COMMENT
            pass 
            self.mDOC_PRE_COMMENT()



        elif alt16 == 34:
            # interface.g:1:267: DOC_POST_COMMENT
            pass 
            self.mDOC_POST_COMMENT()



        elif alt16 == 35:
            # interface.g:1:284: WS
            pass 
            self.mWS()



        elif alt16 == 36:
            # interface.g:1:287: C_COMMENT
            pass 
            self.mC_COMMENT()



        elif alt16 == 37:
            # interface.g:1:297: CPP_COMMENT
            pass 
            self.mCPP_COMMENT()








    # lookup tables for DFA #13

    DFA13_eot = DFA.unpack(
        u"\5\uffff"
        )

    DFA13_eof = DFA.unpack(
        u"\5\uffff"
        )

    DFA13_min = DFA.unpack(
        u"\2\0\3\uffff"
        )

    DFA13_max = DFA.unpack(
        u"\2\uffff\3\uffff"
        )

    DFA13_accept = DFA.unpack(
        u"\2\uffff\1\1\1\3\1\2"
        )

    DFA13_special = DFA.unpack(
        u"\1\1\1\0\3\uffff"
        )


    DFA13_transition = [
        DFA.unpack(u"\52\2\1\1\uffd5\2"),
        DFA.unpack(u"\52\4\1\1\4\4\1\3\uffd0\4"),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u"")
    ]

    # class definition for DFA #13

    class DFA13(DFA):
        pass


        def specialStateTransition(self_, s, input):
            # convince pylint that my self_ magic is ok ;)
            # pylint: disable-msg=E0213

            # pretend we are a member of the recognizer
            # thus semantic predicates can be evaluated
            self = self_.recognizer

            _s = s

            if s == 0: 
                LA13_1 = input.LA(1)

                s = -1
                if (LA13_1 == 47):
                    s = 3

                elif (LA13_1 == 42):
                    s = 1

                elif ((0 <= LA13_1 <= 41) or (43 <= LA13_1 <= 46) or (48 <= LA13_1 <= 65535)):
                    s = 4

                if s >= 0:
                    return s
            elif s == 1: 
                LA13_0 = input.LA(1)

                s = -1
                if (LA13_0 == 42):
                    s = 1

                elif ((0 <= LA13_0 <= 41) or (43 <= LA13_0 <= 65535)):
                    s = 2

                if s >= 0:
                    return s

            nvae = NoViableAltException(self_.getDescription(), 13, _s, input)
            self_.error(nvae)
            raise nvae

    # lookup tables for DFA #16

    DFA16_eot = DFA.unpack(
        u"\10\uffff\1\41\1\uffff\2\43\5\uffff\12\43\1\34\4\uffff\1\66\1\uffff"
        u"\1\43\1\uffff\1\43\1\uffff\2\43\1\72\11\43\3\uffff\1\66\1\uffff"
        u"\3\43\1\uffff\1\112\10\43\2\uffff\1\126\3\43\1\uffff\3\43\1\135"
        u"\4\43\2\uffff\1\126\1\uffff\5\43\1\151\1\uffff\4\43\2\uffff\2\43"
        u"\1\161\2\43\1\uffff\1\43\1\165\2\43\1\uffff\2\43\1\uffff\1\43\1"
        u"\173\1\43\1\uffff\1\175\3\43\1\u0081\1\uffff\1\43\1\uffff\1\u0083"
        u"\2\43\1\uffff\1\u0086\1\uffff\2\43\1\uffff\1\u0089\1\43\1\uffff"
        u"\3\43\1\u008e\1\uffff"
        )

    DFA16_eof = DFA.unpack(
        u"\u008f\uffff"
        )

    DFA16_min = DFA.unpack(
        u"\1\11\7\uffff\1\52\1\uffff\2\56\5\uffff\12\56\1\130\3\uffff\1\0"
        u"\1\57\1\uffff\1\56\1\uffff\1\56\1\uffff\14\56\1\uffff\1\0\1\uffff"
        u"\1\74\1\uffff\3\56\1\uffff\11\56\1\0\1\uffff\1\0\3\56\1\uffff\10"
        u"\56\3\0\1\uffff\6\56\1\uffff\4\56\2\0\5\56\1\uffff\4\56\1\uffff"
        u"\2\56\1\uffff\3\56\1\uffff\5\56\1\uffff\1\56\1\uffff\3\56\1\uffff"
        u"\1\56\1\uffff\2\56\1\uffff\2\56\1\uffff\4\56\1\uffff"
        )

    DFA16_max = DFA.unpack(
        u"\1\175\7\uffff\1\57\1\uffff\2\172\5\uffff\12\172\1\170\3\uffff"
        u"\1\uffff\1\57\1\uffff\1\172\1\uffff\1\172\1\uffff\14\172\1\uffff"
        u"\1\uffff\1\uffff\1\74\1\uffff\3\172\1\uffff\11\172\1\uffff\1\uffff"
        u"\1\uffff\3\172\1\uffff\10\172\3\uffff\1\uffff\6\172\1\uffff\4\172"
        u"\2\uffff\5\172\1\uffff\4\172\1\uffff\2\172\1\uffff\3\172\1\uffff"
        u"\5\172\1\uffff\1\172\1\uffff\3\172\1\uffff\1\172\1\uffff\2\172"
        u"\1\uffff\2\172\1\uffff\4\172\1\uffff"
        )

    DFA16_accept = DFA.unpack(
        u"\1\uffff\1\1\1\2\1\3\1\4\1\5\1\6\1\7\1\uffff\1\11\2\uffff\1\14"
        u"\1\15\1\16\1\17\1\20\13\uffff\1\36\1\40\1\43\2\uffff\1\10\1\uffff"
        u"\1\34\1\uffff\1\35\14\uffff\1\37\1\uffff\1\44\1\uffff\1\45\3\uffff"
        u"\1\21\12\uffff\1\41\4\uffff\1\22\13\uffff\1\42\6\uffff\1\30\13"
        u"\uffff\1\25\4\uffff\1\41\2\uffff\1\32\3\uffff\1\27\5\uffff\1\24"
        u"\1\uffff\1\31\3\uffff\1\23\1\uffff\1\33\2\uffff\1\26\2\uffff\1"
        u"\12\4\uffff\1\13"
        )

    DFA16_special = DFA.unpack(
        u"\37\uffff\1\0\23\uffff\1\1\20\uffff\1\6\1\uffff\1\3\14\uffff\1"
        u"\5\1\4\1\2\14\uffff\1\7\1\10\53\uffff"
        )


    DFA16_transition = [
        DFA.unpack(u"\2\36\25\uffff\1\36\1\uffff\1\35\4\uffff\1\35\1\1\1"
        u"\2\1\3\1\4\1\5\1\6\1\7\1\10\1\33\11\34\1\uffff\1\20\1\uffff\1\11"
        u"\3\uffff\1\12\1\30\1\32\1\27\1\25\1\23\1\32\1\24\1\21\5\32\1\22"
        u"\2\32\1\26\1\13\1\32\1\31\5\32\1\14\1\uffff\1\15\3\uffff\32\32"
        u"\1\16\1\uffff\1\17"),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u"\1\37\4\uffff\1\40"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\17\44\1\42\12\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\46\16\44\1\47\6"
        u"\44\4\uffff\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\50\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\24\44\1\51\5\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\24\44\1\52\5\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\1\53\31\44\4\uffff\1\44"
        u"\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\55\7\44\1\54\4"
        u"\44\4\uffff\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\56\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\57\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\60\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\22\44\1\61\7\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\62\37\uffff\1\62"),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u""),
        DFA.unpack(u"\52\64\1\63\uffd5\64"),
        DFA.unpack(u"\1\65"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\67\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\70\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\71\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\73\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\74\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\75\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\76\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\24\44\1\77\5\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\5\44\1\100\24\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\5\44\1\101\24\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\102\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\103\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\52\104\1\64\4\104\1\105\uffd0\104"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\106"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\107\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\25\44\1\110\4\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\24\44\1\111\5\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\2\44\1\113\27\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\3\44\1\114\26\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\115\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\14\44\1\116\15\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\117\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\120\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\14\44\1\121\15\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\122\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\52\123\1\124\uffd5\123"),
        DFA.unpack(u""),
        DFA.unpack(u"\12\125\1\uffff\ufff5\125"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\25\44\1\127\4\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\130\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\2\44\1\131\27\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\132\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\13\44\1\133\16\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\134\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\136\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\137\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\1\140\31\44\4\uffff\1\44"
        u"\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\30\44\1\141\1\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\52\123\1\142\uffd5\123"),
        DFA.unpack(u"\52\143\1\124\4\143\1\64\uffd0\143"),
        DFA.unpack(u"\12\125\1\uffff\ufff5\125"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\144\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\145\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\23\44\1\146\6\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\147\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\150\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\152\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\153\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\22\44\1\154\7\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\17\44\1\155\12\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\52\143\1\124\4\143\1\156\uffd0\143"),
        DFA.unpack(u"\52\123\1\142\uffd5\123"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\157\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\6\44\1\160\23\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\16\44\1\162\13\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\163\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\164\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\12\44\1\166\17\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\167\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\22\44\1\170\7\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\171\1\uffff"
        u"\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\172\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\2\44\1\174\27\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\22\44\1\176\7\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\177\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\25\44\1\u0080\4\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\u0082\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\16\44\1\u0084\13\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\4\44\1\u0085\25\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\u0087\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\21\44\1\u0088\10\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\22\44\1\u008a\7\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u""),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\10\44\1\u008b\21\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\16\44\1\u008c\13\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\15\44\1\u008d\14\44\4\uffff"
        u"\1\44\1\uffff\32\44"),
        DFA.unpack(u"\1\45\1\uffff\12\44\7\uffff\32\44\4\uffff\1\44\1\uffff"
        u"\32\44"),
        DFA.unpack(u"")
    ]

    # class definition for DFA #16

    class DFA16(DFA):
        pass


        def specialStateTransition(self_, s, input):
            # convince pylint that my self_ magic is ok ;)
            # pylint: disable-msg=E0213

            # pretend we are a member of the recognizer
            # thus semantic predicates can be evaluated
            self = self_.recognizer

            _s = s

            if s == 0: 
                LA16_31 = input.LA(1)

                s = -1
                if (LA16_31 == 42):
                    s = 51

                elif ((0 <= LA16_31 <= 41) or (43 <= LA16_31 <= 65535)):
                    s = 52

                if s >= 0:
                    return s
            elif s == 1: 
                LA16_51 = input.LA(1)

                s = -1
                if ((0 <= LA16_51 <= 41) or (43 <= LA16_51 <= 46) or (48 <= LA16_51 <= 65535)):
                    s = 68

                elif (LA16_51 == 47):
                    s = 69

                elif (LA16_51 == 42):
                    s = 52

                if s >= 0:
                    return s
            elif s == 2: 
                LA16_85 = input.LA(1)

                s = -1
                if ((0 <= LA16_85 <= 9) or (11 <= LA16_85 <= 65535)):
                    s = 85

                else:
                    s = 86

                if s >= 0:
                    return s
            elif s == 3: 
                LA16_70 = input.LA(1)

                s = -1
                if ((0 <= LA16_70 <= 9) or (11 <= LA16_70 <= 65535)):
                    s = 85

                else:
                    s = 86

                if s >= 0:
                    return s
            elif s == 4: 
                LA16_84 = input.LA(1)

                s = -1
                if ((0 <= LA16_84 <= 41) or (43 <= LA16_84 <= 46) or (48 <= LA16_84 <= 65535)):
                    s = 99

                elif (LA16_84 == 42):
                    s = 84

                elif (LA16_84 == 47):
                    s = 52

                if s >= 0:
                    return s
            elif s == 5: 
                LA16_83 = input.LA(1)

                s = -1
                if (LA16_83 == 42):
                    s = 98

                elif ((0 <= LA16_83 <= 41) or (43 <= LA16_83 <= 65535)):
                    s = 83

                if s >= 0:
                    return s
            elif s == 6: 
                LA16_68 = input.LA(1)

                s = -1
                if ((0 <= LA16_68 <= 41) or (43 <= LA16_68 <= 65535)):
                    s = 83

                elif (LA16_68 == 42):
                    s = 84

                if s >= 0:
                    return s
            elif s == 7: 
                LA16_98 = input.LA(1)

                s = -1
                if (LA16_98 == 47):
                    s = 110

                elif ((0 <= LA16_98 <= 41) or (43 <= LA16_98 <= 46) or (48 <= LA16_98 <= 65535)):
                    s = 99

                elif (LA16_98 == 42):
                    s = 84

                if s >= 0:
                    return s
            elif s == 8: 
                LA16_99 = input.LA(1)

                s = -1
                if (LA16_99 == 42):
                    s = 98

                elif ((0 <= LA16_99 <= 41) or (43 <= LA16_99 <= 65535)):
                    s = 83

                if s >= 0:
                    return s

            nvae = NoViableAltException(self_.getDescription(), 16, _s, input)
            self_.error(nvae)
            raise nvae

 



def main(argv, stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr):
    from antlr3.main import LexerMain
    main = LexerMain(interfaceLexer)

    main.stdin = stdin
    main.stdout = stdout
    main.stderr = stderr
    main.execute(argv)



if __name__ == '__main__':
    main(sys.argv)
