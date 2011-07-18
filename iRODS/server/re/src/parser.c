/* For copyright information please refer to files in the COPYRIGHT directory
 */

#include <stdlib.h>
#include "utils.h"
#include "parser.h"
#include "rules.h"
#include "functions.h"
#include "configuration.h"


Op new_ops[num_ops] = {
    {"-",1,10},
    {"++",2,6},
    {"+",2,6},
    {"-",2,6},
    {"*",2,7},
    {"/",2,7},
    {"&&",2,3},
    {"%%",2,2},
    {"||",2,2},
    {"%",2,7},
    {"<=",2,5},
    {">=",2,5},
    {"<",2,5},
    {">",2,5},
    {"==",2,4},
    {"!=",2,4},
    {"!",1,10},
    {"like regex",2,8},
    {"not like regex",2,8},
    {"like",2,8},
    {"not like",2,8},
    {"^^",2,8},
    {"^",2,8},
    {"floor",1,10},
    {"ceiling",1,10},
    {"log",1,10},
    {"exp",1,10},
    {"abs",1,10},
    {"=",2,1},
    {"@@",2,20}
};
PARSER_FUNC_PROTO2(Term, int rulegen, int prec);
PARSER_FUNC_PROTO2(Actions, int rulegen, int backwardCompatible);
PARSER_FUNC_PROTO1(Value, int rulegen);
PARSER_FUNC_PROTO(StringExpression);
PARSER_FUNC_PROTO(RuleName);
PARSER_FUNC_PROTO(TermBackwardCompatible);
PARSER_FUNC_PROTO1(TermSystemBackwardCompatible, int lev);
PARSER_FUNC_PROTO(ActionsBackwardCompatible);
PARSER_FUNC_PROTO(ActionArgumentBackwardCompatible);
PARSER_FUNC_PROTO(FuncExpr);
PARSER_FUNC_PROTO(Type);
PARSER_FUNC_PROTO2(_Type, int prec, int lifted);
PARSER_FUNC_PROTO(TypeSet);
PARSER_FUNC_PROTO(FuncType);
PARSER_FUNC_PROTO(_FuncType);
PARSER_FUNC_PROTO(TypingConstraints);
PARSER_FUNC_PROTO(_TypingConstraints);
PARSER_FUNC_PROTO(Metadata);

/***** utility functions *****/
ParserContext *newParserContext(rError_t *errmsg, Region *r) {
    ParserContext *pc = (ParserContext *)malloc(sizeof(ParserContext));
    pc->stackTopStackTop = 0;
    pc->nodeStackTop = 0;
    pc->error = 0;
    pc->errloc.exprloc = -1;
    pc->region = r;
    pc->errmsg = errmsg;
    pc->errnode = NULL;
    pc->symtable = newHashTable2(100, r);
    pc->errmsgbuf[0] = '\0';
    pc->tqp = pc->tqtop = pc->tqbot = 0;
    return pc;
}

void deleteParserContext(ParserContext *t) {
    free(t);
}
RuleDesc *newRuleDesc(RuleType rk, Node *n, Region *r) {
    RuleDesc *rd = (RuleDesc *) region_alloc(r, sizeof(RuleDesc));
    rd->id = -1;
    rd->node = n;
    rd->type = NULL;
    rd->ruleType = rk;
    return rd;
}

RuleSet *newRuleSet(Region *r) {
    RuleSet *rs = (RuleSet *) region_alloc(r, sizeof(RuleSet));
    rs->len = 0;
    return rs;
}

void setBase(Node *node, char *base, Region *r) {
        node->base= (char *)region_alloc(r, sizeof(char)*(strlen(base)+1));
        strcpy(node->base, base);

}
/**
 * set the degree of a node and allocate subtrees array
 */
Node **setDegree(Node *node, int d, Region *r) {
	node->degree = d;
	node->subtrees = (Node **)region_alloc(r,d*sizeof(Node *));
	if(node->subtrees==NULL) {
		return NULL;
	}
	return node->subtrees;
}
/*
Node *dupNode(Node *node, Region* r) {
    Node *dup = newNode(node->type, node->text, node->expr, r);
    Node **subdup = setDegree(dup, node->degree, r);
    int i;
    for(i = 0;i<node->degree;i++) {
        subdup[i] = dupNode(node->subtrees[i],r);
    }
    return dup;
}*/
Node *createUnaryFunctionNode(char *fn, Node *a, Label * expr, Region *r) {
	Node *node = newNode(N_APPLICATION, fn, expr, r);
	if(node == NULL) {
		return NULL;
	}
	setDegree(node, 1,r);
	node->subtrees[0] = a;
	return node;
}
Node *createBinaryFunctionNode(char *fn, Node *a, Node *b, Label * expr, Region *r) {
	Node *node = newNode(N_APPLICATION, fn, expr,r);
	if(node == NULL) {
		return NULL;
	}
	setDegree(node, 2, r);
	node->subtrees[0] = a;
	node->subtrees[1] = b;
	return node;
}
Node *createFunctionNode(char *fn, Node **params, int paramsLen, Label * exprloc, Region *r) {
	Node *node = newNode(N_APPLICATION, fn, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	Node *func = newNode(TK_TEXT, fn, exprloc, r);
	Node *param = newNode(N_TUPLE, APPLICATION, exprloc, r);
	setDegree(param, paramsLen, r);
	memcpy(param->subtrees, params, paramsLen*sizeof(Node *));
	setDegree(node, 2, r);
	node->subtrees[0] = func;
	node->subtrees[1] = param;
	return node;
}
Node *createActionsNode(Node **params, int paramsLen, Label * exprloc, Region *r) {
	Node *node = newNode(N_ACTIONS, "ACTIONS", exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	setDegree(node, paramsLen,r);
	memcpy(node->subtrees, params, paramsLen*sizeof(Node *));
	return node;
}
Node *createTextNode(char *t, Label * exprloc, Region *r) {
	Node *node = newNode(TK_TEXT, t, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	return node;
}
Node *createIntNode(char *t, Label * exprloc, Region *r) {
	Node *node = newNode(TK_INT, t, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	return node;
}
Node *createDoubleNode(char *t, Label * exprloc, Region *r) {
	Node *node = newNode(TK_DOUBLE, t, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	return node;
}
Node *createStringNode(char *t, Label * exprloc, Region *r) {
	Node *node = newNode(TK_STRING, t, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	return node;
}
Node *createErrorNode(char *error, Label * exprloc, Region *r) {
	Node *node = newNode(N_ERROR, error, exprloc, r);
	if(node == NULL) {
		return NULL;
	}
	return node;
}
int isLocalVariableNode(Node *node) {
    return
        node->nodeType==TK_VAR &&
        node->text[0] == '*';
}
int isSessionVariableNode(Node *node) {
    return
        node->nodeType==TK_VAR &&
        node->text[0] == '$';
}
int isVariableNode(Node *node) {
    return
        isLocalVariableNode(node) ||
        isSessionVariableNode(node);
}
#define nKeywords 19
char *keywords[nKeywords] = { "in", "let", "match", "with", "for", "forExec", "while", "whileExec", "foreach", "forEachExec", "if", "ifExec", "then", "else", "data", "constructor", "on", "or", "oron"};
int isKeyword(char *text) {
	int i;
	for(i=0;i<nKeywords;i++) {
		if(strcmp(keywords[i], text) == 0) {
			return 1;
		}
	}
	return 0;
}

void skipWhitespace(Pointer *expr) {
	char ch;
	ch = lookAhead(expr, 0);
	while (ch!=-1 && (ch==' ' || ch=='\t' || ch=='\r' || ch=='\n')) {
		ch = nextChar(expr);
	}
}
void skipComments(Pointer *e) {
    char ch = lookAhead(e, 0);
    while(ch!='\n' && ch!=-1) {
        ch = nextChar(e);
    }
}
char *findLineCont(char *expr) {
	char *e = expr + strlen(expr);
	while (e!=expr) {
		e--;
		if(*e==' ' || *e=='\t' || *e=='\r' || *e=='\n') continue;
		if(*e=='\\' || *e==',' || *e==';' || *e=='|' || *e=='#') return e;
		break;
	}
	return NULL;
}

/**
 * skip a token of type TK_TEXT, TK_OP, or TK_MISC_OP and text text, token will have type N_ERROR if the token does not match
 * return 0 failure non 0 success
 */
int skip(Pointer *e, char *text, Token **token, ParserContext *pc, int rulegen) {
	*token = nextTokenRuleGen(e, pc, rulegen);
	if(((*token)->type!=TK_TEXT && (*token)->type!=TK_OP && (*token)->type!=TK_MISC_OP) || strcmp((*token)->text,text)!=0) {
		(*token)->type = N_ERROR;
		return 0;
	}
	return 1;
}

/*void getRuleCode(char buf[MAX_RULE_LEN], Pointer *e, int rulegen, ParserContext *context) {

}*/

Token* nextTokenRuleGen(Pointer* e, ParserContext *context, int rulegen) {
	if(context->tqp != context->tqtop) {
		Token *token = &(context->tokenQueue[context->tqp]);
		INC_MOD(context->tqp, 1024);
		return token;
	}

	Token* token = &(context->tokenQueue[context->tqp]);
	INC_MOD(context->tqtop, 1024);
	if(context->tqbot == context->tqtop)
		INC_MOD(context->tqbot, 1024);
	context->tqp = context->tqtop;

    while (1) {
        skipWhitespace(e);
        Label start;
        Label pos;
        getFPos(&start, e);
        token->exprloc = start.exprloc;
        char ch = lookAhead(e, 0);
        if (ch == -1) { /* reach the end of stream */
            token->type = TK_EOS;
            strcpy(token->text, "EOS");
            break;
        } else {
            int i;
            if (ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == ',' || ch == '@' || (ch == '|' && (!rulegen || lookAhead(e, 1) != '|')) || ch == ';' || ch == '?') {
                *(token->text) = ch;
                (token->text)[1] = '\0';
                token->type = TK_MISC_OP;
                nextChar(e);
                break;
            } else if (ch == '#') {
                if (!rulegen && lookAhead(e, 1) == '#') {
                    token->text[0] = ch;
                    token->text[1] = lookAhead(e, 1);
                    token->text[2] = '\0';
                    token->type = TK_MISC_OP;
                    nextChar(e);
                    nextChar(e);
                    break;
                } else {
                    skipComments(e);
                    continue;
                }
            } else if(ch == '-' || ch == '=') {
                if(lookAhead(e,1)=='>') {
                    token->text[0] = ch;
                    token->text[1] = '>';
                    token->text[2] = '\0';
                    token->type = TK_MISC_OP;
                    nextChar(e);
                    nextChar(e);
                    break;
                }
            } else if (ch == ':') {
                if (rulegen && lookAhead(e, 1) == ':'
                        && lookAhead(e, 2) == ':') {
                    token->text[0] = ch;
                    token->text[1] = lookAhead(e, 1);
                    token->text[2] = lookAhead(e, 2);
                    token->text[3] = '\0';
                    token->type = TK_MISC_OP;
                    nextChar(e);
                    nextChar(e);
                } else {
                    *(token->text) = ch;
                    (token->text)[1] = '\0';
                    token->type = TK_MISC_OP;
                }
                nextChar(e);
                break;
            } else if (ch == '*' || ch == '$') { /* variable */
                token->type = ch == '*' ? TK_LOCAL_VAR : TK_SESSION_VAR;

                ch = nextChar(e);
                if (ch == '_' || isalpha(ch)) {
                    ch = nextChar(e);
                    while (ch == '_' || isalnum(ch)) {
                        ch = nextChar(e);
                    }
                    FPOS;
                    dupString(e, &start, (int) (pos.exprloc - start.exprloc), token->text);
                    break;
                } else {
                    seekInFile(e, start.exprloc);
                }
            }
            char op[100];
            dupString(e, &start, 10, op);
            int found = 0;
            for (i = 0; i < num_ops; i++) {
                int len = strlen(new_ops[i].string);
                if (strncmp(op, new_ops[i].string, len) == 0 &&
                        (!isalpha(new_ops[i].string[0])
                        || !isalnum(op[len]))) {
                    strcpy(token->text, new_ops[i].string);
                    token->type = TK_OP;
                    nextChars(e, len);
                    found = 1;
                    break;
                }
            }
            if (found)
                break;
            if (isdigit(ch) || ch == '.') {
                ch = nextChar(e);
                while (isdigit(ch) || ch == '.') {
                    ch = nextChar(e);
                }
                    FPOS;
                    dupString(e, &start, (int) (pos.exprloc - start.exprloc), token->text);
                if (strchr(token->text, '.')) {
                    token->type = TK_DOUBLE;
                } else {
                    token->type = TK_INT;
                }
            } else if (ch == '_' || isalpha(ch) || ch == '~') {
                ch = nextChar(e);
                while (ch == '_' || isalnum(ch)) {
                    ch = nextChar(e);
                }
                    FPOS;
                    dupString(e, &start, (int) (pos.exprloc - start.exprloc), token->text);
                token->type = TK_TEXT;
            } else if (ch == '\"') {
                nextString(e, token->text, token->vars);
                token->type = TK_STRING;
            } else if (ch == '\'') {
                nextString2(e, token->text, token->vars);
                token->type = TK_STRING;
            } else if (ch == '`') {
            	if(lookAhead(e, 1) == '`') {
            		if(nextStringBase2(e, token->text, "``")==-1) {
            			token->type = N_ERROR;
            		} else {
            			token->type = TK_STRING;
            			token->vars[0] = -1;
            		}
            	} else {
					nextStringBase(e, token->text, "`", 1, '\\', token->vars);
					token->type = TK_BACKQUOTED;
            	}
            } else {
                token->type = N_ERROR;
            }
            break;
        }
    }
    return token;
}

void pushback(Pointer *e, Token *token, ParserContext *context) {
    if(token->type == TK_EOS) {
        return;
    }
	DEC_MOD(context->tqp, 1024);
}

int eol(char ch) {
	return ch=='\n' || ch=='\r';
}

/**
 * Parse a rule, create a node pack.
 * If error, either ret==NULL or ret->type==N_ERROR.
 */

PARSER_FUNC_BEGIN1(Rule, int backwardCompatible)
        char *rk;
        int rulegen = 0;
    TRY(defType)
        TTEXT("data");
    	NT(RuleName);
	    BUILD_NODE(N_DATA_DEF, "DATA", &start, 1, 1);
        int n = 0;
        OPTIONAL_BEGIN(consDefs)
        TTEXT("=");
        OPTIONAL_BEGIN(semicolon)
            TTEXT("|");
        OPTIONAL_END(semicolon)
		LOOP_BEGIN(cons)
			Label cpos = *FPOS;
			TTYPE(TK_TEXT);
			BUILD_NODE(TK_TEXT, token->text, &pos, 0, 0);
			TTEXT(":");
			NT(FuncType);
			BUILD_NODE(N_CONSTRUCTOR_DEF, "CONSTR", &cpos, 2, 2);
			n++;
			TRY(delim)
				TTEXT("|");
			OR(delim)
				DONE(cons);
			END_TRY(delim)
		LOOP_END(cons)
        OPTIONAL_END(consDefs)
        OPTIONAL_BEGIN(semicolon)
            TTEXT(";");
        OPTIONAL_END(semicolon)
        n = n+1;
        BUILD_NODE(N_RULE_PACK, "INDUCT", &start, n, n);
	OR(defType)
        TTEXT("constructor");
		TTYPE(TK_TEXT);
		BUILD_NODE(TK_TEXT, token->text, &pos, 0, 0);
	    TTEXT(":")
        NT(FuncType);
	    BUILD_NODE(N_CONSTRUCTOR_DEF, "CONSTR", &start, 2, 2);
        BUILD_NODE(N_RULE_PACK, "CONSTR", &start, 1, 1);
	OR(defType)
        TTYPE(TK_TEXT);
		BUILD_NODE(TK_TEXT, token->text, &pos, 0, 0);
		TTEXT(":");
		NT(FuncType);
	    BUILD_NODE(N_EXTERN_DEF, "EXTERN", &start, 2, 2);
        BUILD_NODE(N_RULE_PACK, "EXTERN", &start, 1, 1);
	OR(defType)
        NT(RuleName);
        TRY(ruleType)
            TTEXT("{");
            rulegen = 1;
            rk = "REL";
        OR(ruleType)
            TTEXT("|");
            rulegen = 0;
            rk = "REL";
        OR(ruleType)
            TTEXT("=");
            rulegen = 1;
            rk = "FUNC";
		END_TRY(ruleType);
        if(strcmp(rk, "FUNC")==0) {
            BUILD_APP_NODE("true", FPOS, 0);
            NT(FuncExpr);
            NT(Metadata);
            OPTIONAL_BEGIN(semicolon)
				TTEXT(";");
            OPTIONAL_END(semicolon)
            BUILD_NODE(N_RULE,"RULE", &start,5, 5);
            BUILD_NODE(N_RULE_PACK,rk, &start,1, 1);
/*        } else if(PARSER_LAZY) {
        	char buf[MAX_RULE_LEN];
        	Label rcpos = *FPOS;
        	getRuleCode(buf, e, rulegen, context);
        	BUILD_NODE(N_RULE_CODE, buf, &rcpos, 0, 0);
            BUILD_NODE(N_UNPARSED,"UNPARSED", &start,2, 2);
            BUILD_NODE(N_RULE_PACK,rk, &start,1, 1); */
        } else if(rulegen) {
            int numberOfRules = 0;
            LOOP_BEGIN(rule)
                TRY(rulePack)
                    TRY(rulePackCond)
                        TTEXT("ON");
                    OR(rulePackCond)
                        TTEXT("on");
                    OR(rulePackCond)
                        TTEXT("ORON");
                    OR(rulePackCond)
                        TTEXT("oron");
                    END_TRY(rulePackCond)
                    NT2(Term, 1, MIN_PREC);
                    TTEXT("{");
                    NT2(Actions, 1, 0);
                    TTEXT("}");
                    NT(Metadata);
                    BUILD_NODE(N_RULE, "RULE", &start, 5, 4);
                    SWAP;
                    numberOfRules++;
                OR(rulePack)
                    TRY(rulePackUncond)
                        TTEXT("OR");
                    OR(rulePackUncond)
                        TTEXT("or");
                    END_TRY(rulePackUncond)
                    BUILD_APP_NODE("true", FPOS, 0);
                    TTEXT("{");
                    NT2(Actions, 1, 0);
                    TTEXT("}");
                    NT(Metadata);
                    BUILD_NODE(N_RULE, "RULE", &start, 5, 4);
                    SWAP;
                    numberOfRules++;
                OR(rulePack)
                    ABORT(numberOfRules == 0);
                    TTEXT("}");
                    DONE(rule);
                OR(rulePack)
                    ABORT(numberOfRules != 0);
                	BUILD_APP_NODE("true", FPOS, 0);
                    NT2(Actions, 1, 0);
                    TTEXT("}");
                    NT(Metadata);
                    numberOfRules = 1;
                    BUILD_NODE(N_RULE, "RULE", &start, 5, 4);
                    SWAP;
                    DONE(rule);
                END_TRY(rulePack)
            LOOP_END(rule)
            (void) POP;
            BUILD_NODE(N_RULE_PACK, rk, &start, numberOfRules, numberOfRules);
        } else {
        	Label pos = *FPOS;
            TRY(ruleCond)
                /* empty condition */
                TTEXT("|");
            	BUILD_APP_NODE("true", &pos, 0);
            OR(ruleCond)
                NT2(Term, 0, MIN_PREC);
                TTEXT("|");
                BUILD_NODE(N_TUPLE, TUPLE, &pos, 1, 1);
            END_TRY(ruleCond)


            NT2(Actions, 0, backwardCompatible);
            TTEXT("|");
            NT2(Actions, 0, backwardCompatible);
            int n = 0;
            Label metadataStart = *FPOS;
            OPTIONAL_BEGIN(ruleId)
            	TTEXT("|");
				TTYPE(TK_INT);
            	BUILD_NODE(TK_STRING,"id", &pos, 0, 0);
            	BUILD_NODE(TK_STRING,token->text, &pos, 0, 0);
            	BUILD_NODE(TK_STRING,"", &pos, 0, 0);
            	BUILD_NODE(N_AVU,AVU, &pos, 3, 3);
            	n++;
            OPTIONAL_END(ruleId)
            BUILD_NODE(N_META_DATA, META_DATA, &metadataStart, n, n);
            BUILD_NODE(N_RULE,"RULE", &start, 5, 5);
            BUILD_NODE(N_RULE_PACK, rk, &start, 1, 1);
        }
	/*OR(defType)
		TTYPE(TK_LOCAL_VAR);
		BUILD_NODE(TK_VAR, token->text, &pos, 0, 0);
		TTEXT("=");
        NT2(Term, 0, MIN_PREC);
        BUILD_NODE(N_RULE_PACK, "GLOBAL", &start, 2, 2);*/
    END_TRY(defType)
PARSER_FUNC_END(Rule)

PARSER_FUNC_BEGIN(RuleName)
    int rulegen = 0;
    TTYPE(TK_TEXT);
    char *ruleName = cpStringExt(token->text, context->region);
    Label paramListStart = *FPOS;
    TRY(params)
        TTEXT("(");
        int n = 0;
        LOOP_BEGIN(params)
            NT2(Term, rulegen, MIN_PREC);
            n++;
            CHOICE_BEGIN(paramType)
                BRANCH_BEGIN(paramType)
                    TTEXT(":");
                    NT(Type);
                BRANCH_END(paramType)
                BRANCH_BEGIN(paramType)
                    BUILD_NODE(T_UNSPECED, NULL, FPOS, 0, 0);
                BRANCH_END(paramType)
            CHOICE_END(paramType)
            CHOICE_BEGIN(paramDelim)
                BRANCH_BEGIN(paramDelim)
                    TTEXT(",");
                BRANCH_END(paramDelim)
                BRANCH_BEGIN(paramDelim)
                    TTEXT(")");
                    DONE(params);
                BRANCH_END(paramDelim)
            CHOICE_END(paramDelim)
        LOOP_END(params);
        UNZIP(n);
        BUILD_NODE(N_PARAM_TYPE_LIST, "paramTypelist", &paramListStart, n, n);
        Node *node = POP;
        BUILD_NODE(N_PARAM_LIST, "paramlist", &paramListStart, n, n);
        PUSH(node);
    OR(params)
        OPTIONAL_BEGIN(epl)
			TTEXT("(");
			TTEXT(")");
        OPTIONAL_END(epl)
        BUILD_NODE(N_PARAM_LIST, "paramlist", &paramListStart, 0, 0);
        BUILD_NODE(N_PARAM_TYPE_LIST, "paramTypelist", &paramListStart, 0, 0);
    END_TRY(params)

    TRY(retType)
        TTEXT(":");
        NT(FuncType);
    OR(retType)
        BUILD_NODE(T_UNSPECED, NULL, FPOS, 0, 0);
    END_TRY(retType)
    BUILD_NODE(N_RULE_NAME, ruleName, &start, 3, 3);
PARSER_FUNC_END(RuleName)

PARSER_FUNC_BEGIN(Metadata)
	int rulegen = 1;
	int n = 0;
	LOOP_BEGIN(metadata)
		TRY(avu)
			TTEXT("@");
			TTEXT("(");
			TTYPE(TK_STRING);
			BUILD_NODE(TK_STRING, token->text, &pos, 0, 0);
			TTEXT(",");
			TTYPE(TK_STRING);
			BUILD_NODE(TK_STRING, token->text, &pos, 0, 0);
			TRY(u)
				TTEXT(",");
				TTYPE(TK_STRING);
				BUILD_NODE(TK_STRING, token->text, &pos, 0, 0);
				TTEXT(")");
			OR(u)
				TTEXT(")");
				BUILD_NODE(TK_STRING, "", &pos, 0, 0);
			END_TRY(u)
			BUILD_NODE(N_AVU, AVU, &pos, 3, 3);
			n++;
		OR(avu)
			DONE(metadata);
		END_TRY(avu)
	LOOP_END(metadata)
	BUILD_NODE(N_META_DATA, META_DATA, &start, n, n);
PARSER_FUNC_END(Metadata)


PARSER_FUNC_BEGIN(FuncExpr)
    int rulegen = 1;
    NT2(Term, 1, MIN_PREC);
    CHOICE_BEGIN(reco)
        BRANCH_BEGIN(reco)
            TTEXT(":::");
            NT2(Term, 1, MIN_PREC);
        BRANCH_END(reco)
        BRANCH_BEGIN(reco)
            BUILD_APP_NODE("nop", FPOS, 0);
        BRANCH_END(reco)
    CHOICE_END(reco)
PARSER_FUNC_END(FuncExpr)

PARSER_FUNC_BEGIN2(Actions, int rulegen, int backwardCompatible)
    int n = 0;
    TRY(actions)
        ABORT(backwardCompatible);
        TTEXT_LOOKAHEAD("}");
    OR(actions)
        LOOP_BEGIN(actions);
            if(!backwardCompatible) {
                NT2(Term, rulegen, MIN_PREC);
            } else {
                NT(TermBackwardCompatible);
            }
            if(rulegen) {
                CHOICE_BEGIN(reco)
                    BRANCH_BEGIN(reco)
                        TTEXT(":::");
                        NT2(Term, rulegen, MIN_PREC);
                    BRANCH_END(reco)
                    BRANCH_BEGIN(reco)
                        BUILD_APP_NODE("nop", NULL, 0);
                    BRANCH_END(reco)
                CHOICE_END(reco)
            }
            n++;
            if(rulegen) {
                OPTIONAL_BEGIN(actionSemiColon)
                    TTEXT(";");
                OPTIONAL_END(actionSemiColon)
                OPTIONAL_BEGIN(actionSemiColonBrace)
                    TTEXT_LOOKAHEAD("}");
                    DONE(actions);
                OPTIONAL_END(actionSemiColonBrace)
            } else {
                CHOICE_BEGIN(actionDelim)
                    BRANCH_BEGIN(actionDelim)
                        TTEXT("##");
                    BRANCH_END(actionDelim)
                    BRANCH_BEGIN(actionDelim)
                        DONE(actions);
                    BRANCH_END(actionDelim)
                CHOICE_END(actionDelim)
            }
        LOOP_END(actions)
    END_TRY(actions)
    if(rulegen)
        UNZIP(n);
    BUILD_NODE(N_ACTIONS, "ACTIONS", &start, n, n);
    if(rulegen) {
        Node *node = POP;
        BUILD_NODE(N_ACTIONS, "ACTIONS", &start, n, n);
        PUSH(node);
    }
PARSER_FUNC_END(Actions)

PARSER_FUNC_BEGIN(ActionsToStrings)
    int rulegen = 1;
    int n = 0;
#define bufferSize 10000
    char actiBuffer[bufferSize], recoBuffer[bufferSize];
    int actiP = 0, recoP = 0;
    Label start, finish;
    TRY(actions)
        TTEXT_LOOKAHEAD("}");
    OR(actions)
        LOOP_BEGIN(actions);
            start = *FPOS;
            NT2(Term, rulegen, MIN_PREC);
            (void) POP;
            finish = *FPOS;
            ABORT(actiP + finish.exprloc - start.exprloc + 1 >= bufferSize);
            dupString(e, &start, finish.exprloc - start.exprloc, actiBuffer + actiP);
            actiP += finish.exprloc - start.exprloc;
            actiBuffer[actiP++] = ';';
            if(rulegen) {
                CHOICE_BEGIN(reco)
                    BRANCH_BEGIN(reco)
                        TTEXT(":::");
                        start = *FPOS;
                        NT2(Term, rulegen, MIN_PREC);
                        (void) POP;
                        finish = *FPOS;
                        ABORT(finish.exprloc - start.exprloc + 1 + recoP >= bufferSize);
                        dupString(e, &start, finish.exprloc - start.exprloc, recoBuffer + recoP);
                        recoP += finish.exprloc - start.exprloc;
                        recoBuffer[actiP++] = ';';
                    BRANCH_END(reco)
                    BRANCH_BEGIN(reco)
                        ABORT(recoP + 4 >= bufferSize);
                        strcpy(recoBuffer + recoP, "nop;");
                        recoP += 4;
                    BRANCH_END(reco)
                CHOICE_END(reco)
            }
            n++;
            if(rulegen) {
                OPTIONAL_BEGIN(actionSemiColon)
                    TTEXT(";");
                OPTIONAL_END(actionSemiColon)
                OPTIONAL_BEGIN(actionSemiColonBrace)
                    TTEXT_LOOKAHEAD("}");
                    DONE(actions);
                OPTIONAL_END(actionSemiColonBrace)
            } else {
                CHOICE_BEGIN(actionDelim)
                    BRANCH_BEGIN(actionDelim)
                        TTEXT("##");
                    BRANCH_END(actionDelim)
                    BRANCH_BEGIN(actionDelim)
                        DONE(actions);
                    BRANCH_END(actionDelim)
                CHOICE_END(actionDelim)
            }
        LOOP_END(actions)
    END_TRY(actions)
    actiBuffer[actiP] = recoBuffer[recoP] = '\0';
    BUILD_NODE(TK_STRING, actiBuffer, &start, 0, 0);
    BUILD_NODE(TK_STRING, recoBuffer, &start, 0, 0);
PARSER_FUNC_END(ActionsToStrings)

PARSER_FUNC_BEGIN1(TermSystemBackwardCompatible, int level)
    int rulegen = 0;
    TRY(func)
        TTEXT("ifExec");
        TTEXT("(");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(")");
        BUILD_APP_NODE("if", &start, 5);

    OR(func)
        TTEXT("whileExec");
        TTEXT("(");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(")");
        BUILD_APP_NODE("while", &start, 3);
    OR(func)
        TTEXT("forEachExec");
        TTEXT("(");
        TTYPE(TK_LOCAL_VAR);
        BUILD_NODE(TK_VAR, token->text, &start, 0, 0);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(")");
        BUILD_APP_NODE("foreach", &start, 3);

    OR(func)
        TTEXT("assign");
        TTEXT("(");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Term, 0, MIN_PREC);
        TTEXT(")");
        BUILD_APP_NODE("assign", &start, 2);
    OR(func)
        TTEXT("forExec");
        TTEXT("(");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Term, 0, MIN_PREC);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(",");
        NT2(Actions, 0, level);
        TTEXT(")");
        BUILD_APP_NODE("for", &start, 5);
    OR(func)
        TTEXT("breakExec");
        BUILD_APP_NODE("break", &start, 0);
    END_TRY(func)
PARSER_FUNC_END(TermSystemBackwardCompatible)

PARSER_FUNC_BEGIN(TermBackwardCompatible)
    int rulegen =0;
    TRY(func)
        NT1(TermSystemBackwardCompatible, 1);

    OR(func)
    	TTYPE(TK_TEXT);
        char *fn = cpStringExt(token->text, context->region);
        TTEXT("(");
        TTEXT(")");
        BUILD_APP_NODE(fn, &start, 0);

    OR(func)
        TTYPE(TK_TEXT);
        char *fn = cpStringExt(token->text, context->region);
        TTEXT("(");
        int n = 0;
        LOOP_BEGIN(func)
            NT(ActionArgumentBackwardCompatible);
            n++;
            CHOICE_BEGIN(paramDelim)
                BRANCH_BEGIN(paramDelim)
                TTEXT(",");
                BRANCH_END(paramDelim)
                BRANCH_BEGIN(paramDelim)
                TTEXT(")");
                DONE(func);
                BRANCH_END(paramDelim)
            CHOICE_END(paramDelim)
        LOOP_END(func)
        BUILD_APP_NODE(fn, &start, n);
    OR(func)
		TTYPE(TK_TEXT);
        char *fn = cpStringExt(token->text, context->region);
        BUILD_APP_NODE(fn, &start, 0);

    END_TRY(func)
PARSER_FUNC_END(ValueBackwardCompatible)


PARSER_FUNC_BEGIN(ActionArgumentBackwardCompatible)
	Token strtoken;
    nextActionArgumentStringBackwardCompatible(e, &strtoken);
    if(strtoken.type != TK_STRING) {
        BUILD_NODE(N_ERROR, "reached the end of stream while parsing an action argument", FPOS,0,0);
    } else {
        NT(StringExpression);
    }
PARSER_FUNC_END(ActionArgumentBackwardCompatible)

PARSER_FUNC_BEGIN2(Term, int rulegen, int prec)
    NT1(Value, rulegen);
    int done = 0;
    while (!done && NO_SYNTAX_ERROR) {
        CHOICE_BEGIN(term)
            BRANCH_BEGIN(term)
                TTYPE(TK_OP);
                ABORT(!isBinaryOp(token));
                if(prec>=getBinaryPrecedence(token)) {
                    PUSHBACK;
                    done = 1;
                } else {
                    char *fn;
                    if(TOKEN_TEXT("=")) {
                        fn = "assign";
                    } else {
                        fn = token->text;
                    }
                    NT2(Term, rulegen, getBinaryPrecedence(token));
#ifdef DEBUG_VERBOSE
                    char err[1024];
                    generateErrMsg(fn, FPOS->exprloc, "ftest", err);
                    printf("%s", err);
#endif
                    BUILD_APP_NODE(fn, &start, 2);
                }

            BRANCH_END(term)
            BRANCH_BEGIN(term)
            	TRY(syntacticalArg)
            		TTEXT_LOOKAHEAD("(");
            	OR(syntacticalArg)
            		TTEXT_LOOKAHEAD("[");
            	END_TRY(syntacticalArg)

                Token appToken;
                strcpy(appToken.text, "@@");
                NT2(Term, rulegen, getBinaryPrecedence(&appToken));
                BUILD_NODE(N_APPLICATION, APPLICATION, &start, 2, 2);
            BRANCH_END(term)
            BRANCH_BEGIN(term)
                done = 1;
            BRANCH_END(term)
        CHOICE_END(term)
    }
    if(!done) {
        break;
    }
PARSER_FUNC_END(Term)

PARSER_FUNC_BEGIN(StringExpression)
    int rulegen = 0;
    TTYPE(TK_STRING);
    Token *strToken = token;
    int i = 0, k = 0;
    pos.base = e->base;
    long startLoc = strToken->exprloc;
    char *str = strToken->text;
    int st[100];
    int end[100];
    int noi = 1;
    st[0] = 0;
    end[0] = strlen(str);
    while(strToken->vars[i]!=-1) {
        /* this string contains reference to vars */
        int vs = strToken->vars[i];
        i++;
        if(!isalpha(str[vs+1]) && str[vs+1]!='_') {
            continue;
        }
        int ve;
        ve = vs+2;
        while(isalnum(str[ve]) || str[ve]=='_') ve++;
        end[noi]=end[noi-1];
        end[noi-1]=vs;
        st[noi]=ve;
        noi++;
    }
    char sbuf[MAX_COND_LEN];
    char delim[1];
    delim[0] = '\0';
    strncpy(sbuf, str+st[0], end[0]-st[0]);
    strcpy(sbuf+end[0]-st[0], delim);
    pos.exprloc = startLoc + st[0];
    int startloc = pos.exprloc;
    BUILD_NODE(TK_STRING, sbuf, &pos,0,0);
    for(k=1;k<noi;k++) {
        strncpy(sbuf, str+end[k-1], st[k]-end[k-1]); /* var */
        strcpy(sbuf+st[k]-end[k-1], delim);
        pos.exprloc = startLoc + end[k-1];
        BUILD_NODE(TK_VAR, sbuf, &pos, 0, 0);

        BUILD_APP_NODE("str", &pos, 1);

        pos.exprloc = startloc;
        BUILD_APP_NODE("++", &pos, 2);

        strncpy(sbuf, str+st[k], end[k]-st[k]);
        strcpy(sbuf+end[k]-st[k], delim);
        pos.exprloc = startLoc + st[k];
        BUILD_NODE(TK_STRING, sbuf, &pos, 0, 0);

        pos.exprloc = startloc;
        BUILD_APP_NODE("++", &pos, 2);
    }
PARSER_FUNC_END(StringExpression)

PARSER_FUNC_BEGIN1(Value, int rulegen)
    TRY(value)
        TTYPE(TK_LOCAL_VAR);
        BUILD_NODE(TK_VAR, token->text, &pos,0,0);
    OR(value)
        TTYPE(TK_SESSION_VAR);
        BUILD_NODE(TK_VAR, token->text, &pos,0,0);
    OR(value)
        TTEXT("(");
        TRY(tuple)
            TTEXT(")");
            BUILD_NODE(N_TUPLE, TUPLE, &start, 0, 0);
        OR(tuple)
			NT2(Term, rulegen, MIN_PREC);
        	int n = 1;
			LOOP_BEGIN(tupleLoop)
				TRY(tupleComp)
					TTEXT(",");
					NT2(Term, rulegen, MIN_PREC);
					n++;
				OR(tupleComp)
					TTEXT(")");
					DONE(tupleLoop);
				END_TRY(tupleComp)
			LOOP_END(tupleLoop)
			BUILD_NODE(N_TUPLE, TUPLE, &start, n, n);


        END_TRY(tuple)

    OR(value)
        TTEXT("{");
        NT2(Actions, rulegen, 0);
        if(rulegen)
            BUILD_NODE(N_ACTIONS_RECOVERY, "ACTIONS_RECOVERY", &start, 2,2);
        TTEXT("}");

    OR(value)
        TTYPE(TK_OP);
        ABORT(!isUnaryOp(token));
        NT2(Term, rulegen, getUnaryPrecedence(token));
        char *fn;
        if(strcmp(token->text, "-")==0) {
            fn = "neg";
        } else {
            fn = token->text;
        }
        BUILD_APP_NODE(fn, &start,1);

    OR(value)
        TRY(func)
            ABORT(!rulegen);
            TRY(funcIf)
				TTEXT("if");
				TTEXT("(");
				NT2(Term, 1, MIN_PREC);
				TTEXT(")");
                OPTIONAL_BEGIN(ifThen)
                    TTEXT("then");
                OPTIONAL_END(ifThen)
                TTEXT("{");
                NT2(Actions, 1, 0);
                TTEXT("}");
                TRY(ifElse)
                    TTEXT("else");
                    TTEXT_LOOKAHEAD("if");
                    NT2(Term, 1, MIN_PREC);
                    BUILD_NODE(N_ACTIONS, "ACTIONS", &pos, 1, 1);
                    BUILD_APP_NODE("nop", FPOS, 0);
                    BUILD_NODE(N_ACTIONS, "ACTIONS", FPOS, 1, 1);
                OR(ifElse)
                    TTEXT("else");
                    TTEXT("{");
                    NT2(Actions, 1, 0);
                    TTEXT("}");
                OR(ifElse)
                    BUILD_APP_NODE("nop", FPOS, 0);
                    BUILD_NODE(N_ACTIONS, "ACTIONS", FPOS, 1, 1);
                    BUILD_APP_NODE("nop", FPOS, 0);
                    BUILD_NODE(N_ACTIONS, "ACTIONS", FPOS, 1, 1);
                END_TRY(ifElse)
                UNZIP(2);
                BUILD_APP_NODE("if", &start, 5);
            OR(funcIf)
                TTEXT("if");
                NT2(Term, rulegen, MIN_PREC);
                TTEXT("then");
                NT2(Term, 1, MIN_PREC);
                BUILD_APP_NODE("nop", FPOS, 0);
                TTEXT("else")
                NT2(Term, 1, MIN_PREC);
                BUILD_APP_NODE("nop", FPOS, 0);
                UNZIP(2);
                BUILD_APP_NODE("if2", &start, 5);
            END_TRY(funcIf)
        OR(func)
            ABORT(!rulegen);
            TRY(whil)
            	TTEXT("while");
            OR(whil)
            	TTEXT("whileExec");
            END_TRY(whil)
            TTEXT("(");
            NT2(Term, 1, MIN_PREC);
            TTEXT(")");
            TTEXT("{");
            NT2(Actions, 1, 0);
            TTEXT("}");
            BUILD_APP_NODE("while", &start, 3);
        OR(func)
            ABORT(!rulegen);
        	TRY(foreach)
            	TTEXT("foreach");
        	OR(foreach)
        		TTEXT("forEachExec");
        	END_TRY(foreach)
            TTEXT("(");
            TTYPE(TK_LOCAL_VAR);
            BUILD_NODE(TK_VAR, token->text, &pos, 0, 0);
            TRY(foreach2)
	            TTEXT(")");
	            TTEXT("{");
	            NT2(Actions, 1, 0);
	            TTEXT("}");
	            BUILD_APP_NODE("foreach", &start, 3);
			OR(foreach2)
				TTEXT("in");
				NT2(Term, 1, MIN_PREC);
				TTEXT(")");
				TTEXT("{");
				NT2(Actions, 1, 0);
				TTEXT("}");
				BUILD_APP_NODE("foreach2", &start, 4);
			END_TRY(foreach2)

        OR(func)
            ABORT(!rulegen);
        	TRY(fo)
            	TTEXT("for");
        	OR(fo)
        		TTEXT("forExec");
        	END_TRY(fo)
            TTEXT("(");
            NT2(Term, 1, MIN_PREC);
            TTEXT(";");
            NT2(Term, 1, MIN_PREC);
            TTEXT(";");
            NT2(Term, 1, MIN_PREC);
            TTEXT(")");
            TTEXT("{");
            NT2(Actions, 1, 0);
            TTEXT("}");
            BUILD_APP_NODE("for", &start, 5);
        OR(func)
            ABORT(!rulegen);
            TTEXT("remote");
            TTEXT("(");
            NT2(Term, 1, MIN_PREC);
            TTEXT(",");
            NT2(Term, 1, MIN_PREC);
            TTEXT(")");
            TTEXT("{");
            char buf[10000];
            Label actionsStart = *FPOS;
            NT2(Actions, 1, 0);
            (void) POP;
            (void) POP;
            Label actionsFinish = *FPOS;
            TTEXT("}");
            dupString(e, &actionsStart, actionsFinish.exprloc - actionsStart.exprloc, buf);
            BUILD_NODE(TK_STRING, buf, &actionsStart, 0, 0);
            BUILD_NODE(TK_STRING, "", &actionsFinish, 0, 0);
            BUILD_APP_NODE("remoteExec", &start, 4);
        OR(func)
            ABORT(!rulegen);
            TTEXT("delay");
            TTEXT("(");
            NT2(Term, 1, MIN_PREC);
            TTEXT(")");
            TTEXT("{");
            char buf[10000];
            Label actionsStart = *FPOS;
            NT2(Actions, 1, 0);
            (void) POP;
            (void) POP;
            Label actionsFinish = *FPOS;
            TTEXT("}");
            dupString(e, &actionsStart, actionsFinish.exprloc - actionsStart.exprloc, buf);
            BUILD_NODE(TK_STRING, buf, &actionsStart, 0, 0);
            BUILD_NODE(TK_STRING, "", &actionsFinish, 0, 0);
            BUILD_APP_NODE("delayExec", &start, 3);
        OR(func)
            ABORT(!rulegen);
            TTEXT("let");
            NT2(Term, 1, 2);
            TTEXT("=");
            NT2(Term, 1, MIN_PREC);
            TTEXT("in");
            NT2(Term, 1, MIN_PREC);
            BUILD_APP_NODE("let", &start, 3);
		OR(func)
			ABORT(!rulegen);
			TTEXT("match");
			NT2(Term, 1, 2);
			TTEXT("with");
			int n = 0;
	        OPTIONAL_BEGIN(semicolon)
	            TTEXT("|");
	        OPTIONAL_END(semicolon)
			LOOP_BEGIN(cases)
				Label cpos = *FPOS;
				NT2(Term, 1, MIN_PREC);
				TTEXT("=>");
				NT2(Term, 1, MIN_PREC);
				BUILD_NODE(N_TUPLE, TUPLE, &cpos, 2, 2);
				n++;
				TRY(vbar)
					TTEXT("|");
				OR(vbar)
					DONE(cases)

				END_TRY(vbar);
			LOOP_END(cases)
			BUILD_APP_NODE("match", &start, n+1);
        OR(func)
            ABORT(rulegen);
            NT1(TermSystemBackwardCompatible, 0);
        OR(func)
            TTYPE(TK_TEXT);
        	ABORT(rulegen && isKeyword(token->text));
            char *fn = cpStringExt(token->text, context->region);
            BUILD_NODE(TK_TEXT, fn, &start, 0, 0);
#ifdef DEBUG_VERBOSE
            char err[1024];
            generateErrMsg(fn, start.exprloc, start.base, err);
            printf("%s, %ld\n", err, start.exprloc);
#endif
            TRY(nullary)
            	TTEXT_LOOKAHEAD("(");
            OR(nullary)
            	TTEXT_LOOKAHEAD("[");
            OR(nullary)
            	Node *n = POP;
				BUILD_APP_NODE(n->text, &start, 0);
        	END_TRY(nullary)
        END_TRY(func)
    OR(value)
        TTYPE(TK_INT);
        BUILD_NODE(TK_INT, token->text, &start, 0, 0);
    OR(value)
        TTYPE(TK_DOUBLE);
        BUILD_NODE(TK_DOUBLE, token->text, &start, 0, 0);
    OR(value)
        NT(StringExpression);
    END_TRY(value)
PARSER_FUNC_END(Value)

int nextStringBase2(Pointer *e, char *value, char* delim) {
		nextChar(e);
    	char ch = nextChar(e);
    	while(ch!=-1) {
			if(delim[0] == ch && delim[1] == lookAhead(e, 1)) {
				if(delim[0] == delim[1]) {
					while(lookAhead(e, 2) == delim[1]) {
						*(value++) = delim[0];
						nextChar(e);
					}
				}
				*value='\0';
				nextChar(e);
				nextChar(e);
				return 0;
			}
    		*(value++) = ch;
    		ch = nextChar(e);
    	}
    	return -1;

}
/*
 * return number of vars or -1 if no string found
 */
int nextStringBase(Pointer *e, char *value, char* delim, int consumeDelim, char escape, int vars[]) {
	int mode=1; /* 1 string 3 escape */
        int nov = 0;
	char* value0=value;
	*value = lookAhead(e, 0);
	value++;
	char ch = nextChar(e);
	while(ch!=-1) {
		*value = ch;
		switch(mode) {
                    case 1:
                        if(ch ==escape) {
                            value--;
                            mode = 3;
                        } else if(strchr(delim, ch)!=NULL) {
                            if(consumeDelim) {
                                value[1]='\0';
                                trimquotes(value0);
                                nextChar(e);
                            } else {
                                value[0]='\0';
                            }
                            vars[nov] = -1;
                            return nov;
                        } else if((ch =='*' || ch=='$') &&
                                isalpha(lookAhead(e, 1))) {
                            vars[nov++] = value - value0 - 1;
                        }


                        break;
                    case 3:
                        if(ch=='n') {
                            *value = '\n';
                        } else if(ch == 't') {
                            *value = '\t';
                        } else if(ch =='r') {
                            *value = '\r';
                        } else if(ch =='0') {
                            *value = '\0';
                        } else {
                            *value = ch;
                        }
                        mode -= 2;
		}
		ch = nextChar(e);
		value ++;
	}
	return -1;
}
int nextString(Pointer *e, char *value, int vars[]) {
	return nextStringBase(e, value, "\"", 1, '\\', vars);
}
int nextString2(Pointer *e, char *value, int vars[]) {
	return nextStringBase(e, value, "\'", 1, '\\', vars);
}

int getBinaryPrecedence(Token * token) {
	int i;
	for(i=0;i<num_ops;i++) {
		if(new_ops[i].arity==2 && strcmp(new_ops[i].string,token->text)==0) {
			return new_ops[i].prec;
		}
	}
	return -1;
}
int getUnaryPrecedence(Token * token) {
	int i;
	for(i=0;i<num_ops;i++) {
		if(new_ops[i].arity==1 && strcmp(new_ops[i].string,token->text)==0) {
			return new_ops[i].prec;
		}
	}
	return -1;
}
int isUnaryOp(Token *token) {
	int i;
	for (i=0;i<num_ops;i++) {
		if (strcmp(token->text, new_ops[i].string) == 0) {
			if(new_ops[i].arity == 1) return 1;
		}
	}
	return 0;
}
int isBinaryOp(Token *token) {
	int i;
	for (i=0;i<num_ops;i++) {
		if (strcmp(token->text, new_ops[i].string) == 0) {
			if(new_ops[i].arity == 2) return 1;
		}
	}
	return 0;
}
int isOp(char *token) {
	int i;
	for (i=0;i<num_ops;i++) {
		if (strcmp(token, new_ops[i].string) == 0) {
			return 1;
		}
	}
	return 0;
}
char* trim(char* str) {
	char* trimmed = str;
	while(*trimmed =='\t' || *trimmed==' ') {
		trimmed ++;
	}
	int l = strlen(trimmed)-1;
	while(l>=0 && (trimmed[l] =='\t' || trimmed[l]==' ')) {
		l--;
	}
	trimmed[l+1] = '\0';
	return trimmed;

}
void trimquotes(char *string) {
	int len = strlen(string)-2;
	memmove(string, string+1, len*sizeof(char));
	string[len]='\0';
}

void printTree(Node *n, int indent) {
	printIndent(indent);
	char buf[128], buf2[128];
	if(n->nodeType >= T_UNSPECED && n->nodeType <= T_TYPE ) {
		typeToString(n, NULL, buf, 128);
		printf("%s:%d\n", buf, n->nodeType);
		return;
	} else if(n->nodeType >= TC_LT && n->nodeType <= TC_SET) {
		printf("%s:%d\n",n->text, n->nodeType);
	} else {
	if(n->coercionType!=NULL) {
	    typeToString(n->coercionType, NULL, buf, 128);
	} else {
	    buf[0] = '\0';
	}
	if(n->exprType!=NULL) {
	    typeToString(n->exprType, NULL, buf2, 128);
	} else {
	    buf2[0] = '\0';
	}
	char iotype[128];
	strcpy(iotype, "");
	if(n->iotype & IO_TYPE_INPUT) {
		strcat(iotype, "i");
	}
	if(n->iotype & IO_TYPE_OUTPUT) {
		strcat(iotype, "o");
	}
	if(n->iotype & IO_TYPE_DYNAMIC) {
		strcat(iotype, "d");
	}
	if(n->iotype & IO_TYPE_EXPRESSION) {
		strcat(iotype, "e");
	}
	if(n->iotype & IO_TYPE_ACTIONS) {
		strcat(iotype, "a");
	}
	printf("%s:%d %s => %s(option=%d)[%s]\n",n->text, n->nodeType, buf2, buf, n->option, iotype);
}
	int i;
	for(i=0;i<n->degree;i++) {
		printTree(n->subtrees[i],indent+1);
	}

}
#define PRINT(p, s, f, d) 	snprintf(*p, *s, f, d);*s -= strlen(*p); *p += strlen(*p);
void patternToString(char **p, int *s, int indent, int prec, Node *n) {
	switch(n->nodeType) {
	case N_APPLICATION:
		if(n->subtrees[0]->nodeType == TK_TEXT) {
			char *fn = n->subtrees[0]->text;
			Token t;
			strcpy(t.text, fn);
			if(isBinaryOp(&t)) {
				int opPrec = getBinaryPrecedence(&t);
				if(opPrec < prec) {
					PRINT(p, s, "%s", "(");
				}
				patternToString(p, s, indent, prec, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, " %s ", fn);
				patternToString(p, s, indent, prec+1, n->subtrees[1]->subtrees[1]);
				if(opPrec < prec) {
					PRINT(p, s, "%s", ")");
				}
			} else {
				patternToString(p, s, indent, MIN_PREC, n->subtrees[0]);
				if(n->subtrees[1]->nodeType != N_TUPLE || n->subtrees[1]->degree != 0) {
					patternToString(p, s, indent, MIN_PREC, n->subtrees[1]);
				}
			}
		} else {
			PRINT(p, s, "%s", "<unsupported>");
		}
		break;
	case N_TUPLE:
		PRINT(p, s, "%s", "(");
		int i;
		for(i=0;i<n->degree;i++) {
			if(i!=0) {
				PRINT(p, s, "%s", ",");
			}
			patternToString(p, s, indent, MIN_PREC, n->subtrees[i]);
		}
		PRINT(p, s, "%s", ")");
		break;
	case TK_BOOL:
	case TK_DOUBLE:
	case TK_INT:
	case TK_VAR:
	case TK_TEXT:
		PRINT(p, s, "%s", n->text);
		break;
	case TK_STRING:
		PRINT(p, s, "%s", "\"");
		unsigned int k;
		for(k=0;k<strlen(n->text);k++) {
			switch(n->text[k]) {
			case '\t':
				PRINT(p, s, "%s", "\\t");
				break;
			case '\n':
				PRINT(p, s, "%s", "\\n");
				break;
			case '\r':
				PRINT(p, s, "%s", "\\r");
				break;
			case '$':
			case '*':
			case '\\':
			case '\"':
			case '\'':
				PRINT(p, s, "%s", "\\");
				PRINT(p, s, "%c", n->text[k]);
				break;
			default:
				PRINT(p, s, "%c", n->text[k]);

			}
		}
		PRINT(p, s, "%s", "\"");
		break;
	default:
		PRINT(p, s, "%s", "<unsupported>");
	}
}
void termToString(char **p, int *s, int indent, int prec, Node *n) {
	switch(n->nodeType) {
	case N_ACTIONS_RECOVERY:
		actionsToString(p, s, indent, n->subtrees[0], n->subtrees[1]);
		break;
	case N_APPLICATION:
		if(n->subtrees[0]->nodeType == TK_TEXT) {
			char *fn = n->subtrees[0]->text;
			if(strcmp(fn, "if") == 0) {
				PRINT(p, s, "%s", "if (");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", ") ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[1], n->subtrees[1]->subtrees[3]);
				PRINT(p, s, "%s", " else ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[2], n->subtrees[1]->subtrees[4]);
				break;
			}
			if(strcmp(fn, "if2") == 0) {
				PRINT(p, s, "%s", "if ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", " then ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[1]);
				PRINT(p, s, "%s", " else ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[2]);
				break;
			}
			if(strcmp(fn, "while") == 0) {
				PRINT(p, s, "%s", "while (");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", ") ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[1], n->subtrees[1]->subtrees[2]);
				break;
			}
			if(strcmp(fn, "foreach") == 0) {
				PRINT(p, s, "%s", "foreach (");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", ") ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[1], n->subtrees[1]->subtrees[2]);
				break;
			}
			if(strcmp(fn, "foreach2") == 0) {
				PRINT(p, s, "%s", "foreach (");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", " in ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[1]);
				PRINT(p, s, "%s", ") ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[2], n->subtrees[1]->subtrees[3]);
				break;
			}
			if(strcmp(fn, "for") == 0) {
				PRINT(p, s, "%s", "for (");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", ";");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[1]);
				PRINT(p, s, "%s", ";");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[2]);
				PRINT(p, s, "%s", ") ");
				actionsToString(p, s, indent, n->subtrees[1]->subtrees[3], n->subtrees[1]->subtrees[4]);
				break;
			}
			if(strcmp(fn, "let") == 0) {
				PRINT(p, s, "%s", "let ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, "%s", " = ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[1]);
				PRINT(p, s, "%s", " in ");
				termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[2]);
				break;
			}
			if(strcmp(fn, "match") == 0) {
				PRINT(p, s, "%s", "match ");
				termToString(p, s, indent, MIN_PREC, N_APP_ARG(n, 0));
				PRINT(p, s, "%s", " with");
				int i;
				for(i=1;i<N_APP_ARITY(n);i++) {
					PRINT(p, s, "%s", "\n");
					indentToString(p, s, indent + 1);
					PRINT(p, s, "%s", "| ");
					patternToString(p, s, indent + 1, MIN_PREC, N_APP_ARG(n, i)->subtrees[0]);
					PRINT(p, s, "%s", " => ");
					termToString(p, s, indent + 1, MIN_PREC, N_APP_ARG(n, i)->subtrees[1]);
				}
				break;
			}
			if(strcmp(fn, "assign") == 0) {
							patternToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[0]);
							PRINT(p, s, "%s", " = ");
							termToString(p, s, indent, MIN_PREC, n->subtrees[1]->subtrees[1]);
							break;
						}
			Token t;
			strcpy(t.text, fn);
			if(isBinaryOp(&t)) {
				int opPrec = getBinaryPrecedence(&t);
				if(opPrec < prec) {
					PRINT(p, s, "%s", "(");
				}
				termToString(p, s, indent, prec, n->subtrees[1]->subtrees[0]);
				PRINT(p, s, " %s ", fn);
				termToString(p, s, indent, prec+1, n->subtrees[1]->subtrees[1]);
				if(opPrec < prec) {
					PRINT(p, s, "%s", ")");
				}
				break;
			}
		}
		termToString(p, s, indent, MIN_PREC, n->subtrees[0]);
		termToString(p, s, indent, MIN_PREC, n->subtrees[1]);
		break;
	case N_TUPLE:
		PRINT(p, s, "%s", "(");
		int i;
		for(i=0;i<n->degree;i++) {
			if(i!=0) {
				PRINT(p, s, "%s", ",");
			}
			termToString(p, s, indent, MIN_PREC, n->subtrees[i]);
		}
		PRINT(p, s, "%s", ")");
		break;
	case TK_BOOL:
	case TK_DOUBLE:
	case TK_INT:
	case TK_VAR:
	case TK_TEXT:
		PRINT(p, s, "%s", n->text);
		break;
	case TK_STRING:
		PRINT(p, s, "%s", "\"");
		unsigned int k;
		for(k=0;k<strlen(n->text);k++) {
			switch(n->text[k]) {
			case '\t':
				PRINT(p, s, "%s", "\\t");
				break;
			case '\n':
				PRINT(p, s, "%s", "\\n");
				break;
			case '\r':
				PRINT(p, s, "%s", "\\r");
				break;
			case '$':
			case '*':
			case '\\':
			case '\"':
			case '\'':
				PRINT(p, s, "%s", "\\");
				PRINT(p, s, "%c", n->text[k]);
				break;
			default:
				PRINT(p, s, "%c", n->text[k]);

			}

		}
		PRINT(p, s, "%s", "\"");
		break;
	default:
		PRINT(p, s, "%s", "<unsupported>");
	}
}
void indentToString(char **p, int *s, int indent) {
	int i;
	for(i=0;i<indent;i++) {
		PRINT(p, s, "%s", "    ");
	}

}
void actionsToString(char **p, int *s, int indent, Node *na, Node *nr) {
	int n = na->degree;

	int i;
	PRINT(p, s, "%s", "{\n");
	for(i=0;i<n;i++) {
		indentToString(p, s, indent+1);
		termToString(p, s, indent+1, MIN_PREC, na->subtrees[i]);
		if(i<nr->degree && (nr->subtrees[i]->nodeType!=N_APPLICATION || strcmp(nr->subtrees[i]->subtrees[0]->text, "nop")!=0)) {
			PRINT(p, s, "%s", ":::");
			termToString(p, s, indent+1, MIN_PREC, nr->subtrees[i]);
		}
		if((*p)[-1]!='}')
		PRINT(p, s, "%s", ";");
		PRINT(p, s, "%s", "\n");
	}
	indentToString(p, s, indent);
	PRINT(p, s, "%s", "}");
}

void metadataToString(char **p, int *s, int indent, Node *nm) {
	int n = nm->degree;
	int i;
	for(i=0;i<n;i++) {
		indentToString(p, s, indent);
		PRINT(p, s, "%s", "@(");
		termToString(p, s, indent, MIN_PREC, nm->subtrees[i]->subtrees[0]);
		PRINT(p, s, "%s", ", ");
		termToString(p, s, indent, MIN_PREC, nm->subtrees[i]->subtrees[1]);
		PRINT(p, s, "%s", ", ");
		termToString(p, s, indent, MIN_PREC, nm->subtrees[i]->subtrees[2]);
		PRINT(p, s, "%s", ")\n");
	}
}

void ruleNameToString(char **p, int *s, int indent, Node *rn) {
	PRINT(p, s, "%s", rn->text);
	PRINT(p, s, "%s", "(");
	int i;
	for(i=0;i<rn->subtrees[0]->degree;i++) {
		if(i!=0) {
			PRINT(p, s, "%s", ",");
		}
		patternToString(p, s, indent, MIN_PREC, rn->subtrees[0]->subtrees[i]);
	}
	PRINT(p, s, "%s", ")");
}
void typeToStringParser(char **p, int *s, int indent, int lifted, ExprType *type) {
        ExprType *etype = type;

        if(getIOType(etype) == (IO_TYPE_INPUT | IO_TYPE_OUTPUT)) {
        	PRINT(p, s, "%s ", "input output");
        } else if(getIOType(etype) == IO_TYPE_OUTPUT) {
        	PRINT(p, s, "%s ", "output");
        } else if(getIOType(etype) == IO_TYPE_DYNAMIC) {
        	PRINT(p, s, "%s ", "dynamic");
        } else if(getIOType(etype) == IO_TYPE_ACTIONS) {
        	PRINT(p, s, "%s ", "actions");
        } else if(getIOType(etype) == IO_TYPE_EXPRESSION) {
        	PRINT(p, s, "%s ", "expression");
        }
        if(etype->nodeType == T_VAR) {
        	PRINT(p, s, "%s", etype->text);
            if(T_VAR_NUM_DISJUNCTS(type)!=0) {
                PRINT(p, s, " %s", "{");
                int i;
                for(i=0;i<T_VAR_NUM_DISJUNCTS(type);i++) {
                    typeToStringParser(p, s, indent, 0, T_VAR_DISJUNCT(type, i));
                    PRINT(p, s, "%s", " ");
                }
                PRINT(p, s, "%s", "}");
            }
        } else if(etype->nodeType == T_CONS) {
        	if(strcmp(etype->text, FUNC) == 0) {
        		/* PRINT(p, s, "%s", "("); */
				typeToStringParser(p, s, indent, 1, T_CONS_TYPE_ARG(etype, 0));
			    if(getVararg(type) == OPTION_VARARG_OPTIONAL) {
			    	PRINT(p, s, " %s", "?");
			    } else if(getVararg(type) == OPTION_VARARG_STAR) {
			    	PRINT(p, s, " %s", "*");
			    } else if(getVararg(type) == OPTION_VARARG_PLUS) {
			    	PRINT(p, s, " %s", "+");
			    }
        		PRINT(p, s, " %s ", "->");
        		typeToStringParser(p, s, indent, 0, T_CONS_TYPE_ARG(etype, 1));
		    	/* PRINT(p, s, "%s", ")"); */
			} else {
				PRINT(p, s, "%s", T_CONS_TYPE_NAME(etype));
				int i;
				if(T_CONS_ARITY(etype) != 0) {
					PRINT(p, s, "%s", "(");
					for(i=0;i<T_CONS_ARITY(etype);i++) {
						if(i!=0) {
							PRINT(p, s, "%s ", ",");
						}
						typeToStringParser(p, s, indent, 0, T_CONS_TYPE_ARG(etype, i));
					}
					PRINT(p, s, "%s", ")");
				}
			}
        } else if(etype->nodeType == T_FLEX) {
            PRINT(p, s, "%s ", typeName_Parser(etype->nodeType));
            typeToStringParser(p, s, indent, 0, etype->subtrees[0]);
        } else if(etype->nodeType == T_FIXD) {
        	PRINT(p, s, "%s ", typeName_Parser(etype->nodeType));
            typeToStringParser(p, s, indent, 0, etype->subtrees[0]);
        	PRINT(p, s, " %s ", "=>");
        	typeToStringParser(p, s, indent, 0, etype->subtrees[1]);
        } else if(etype->nodeType == T_TUPLE) {
        	if(T_CONS_ARITY(etype) == 0) {
        		PRINT(p, s, "%s", "unit");
        	} else {
        		if(T_CONS_ARITY(etype) == 1 && !lifted) {
            		PRINT(p, s, "%s", "<");
        		}
				int i;
				for(i=0;i<T_CONS_ARITY(etype);i++) {
					if(i!=0) {
					    PRINT(p, s, " %s ", "*");
					}
					typeToStringParser(p, s, indent, 0, T_CONS_TYPE_ARG(etype, i));
				}
        		if(T_CONS_ARITY(etype) == 1 && !lifted) {
            		PRINT(p, s, "%s", ">");
        		}
        	}
        } else if(etype->nodeType == T_IRODS) {
        	PRINT(p, s, "`%s`", etype->text);
        } else {
        	PRINT(p, s, "%s", typeName_Parser(etype->nodeType));
        }

}

void ruleToString(char *buf, int size, RuleDesc *rd) {
	Node *node = rd->node;
	char **p = &buf;
	int *s = &size;
	Node *subt = NULL;
	switch(rd->ruleType) {
	case RK_REL:
		ruleNameToString(p, s, 0, node->subtrees[0]);

			int indent;
			subt = node->subtrees[1];
			while(subt->nodeType == N_TUPLE && subt->degree == 1) {
				subt = subt->subtrees[0];
			}

			PRINT(p, s, "%s", " ");
			if(subt->nodeType != N_APPLICATION ||
					subt->subtrees[0]->nodeType != TK_TEXT ||
					strcmp(subt->subtrees[0]->text, "true") != 0) {
				PRINT(p, s, "%s", "{\n");
				indentToString(p, s, 1);
				PRINT(p, s, "%s", "on ");
				termToString(p, s, 1, MIN_PREC, node->subtrees[1]);
				PRINT(p, s, "%s", " ");
				indent = 1;
			} else {
				indent = 0;
			}
			actionsToString(p, s, indent, node->subtrees[2], node->subtrees[3]);
			if(indent == 1) {
				PRINT(p, s, "%s", "\n}");
			}
			PRINT(p, s, "%s", "\n");
			metadataToString(p, s, 0, node->subtrees[4]);
			break;
	case RK_FUNC:
		ruleNameToString(p, s, 0, node->subtrees[0]);
		PRINT(p, s, "%s", " = ");
		termToString(p, s, 1, MIN_PREC, node->subtrees[2]);
		PRINT(p, s, "%s", "\n");
		metadataToString(p, s, 0, node->subtrees[4]);
		break;
	case RK_CONSTRUCTOR:
		PRINT(p, s, "constructor %s", node->subtrees[0]->text);
		PRINT(p, s, "%s", " : ");
		typeToStringParser(p, s, 0, 0, node->subtrees[1]);
		PRINT(p, s, "%s", "\n");
		/* metadataToString(p, s, 0, node->subtrees[2]); */
		break;
	case RK_DATA:
		PRINT(p, s, "%s ", "data");
		ruleNameToString(p, s, 0, node->subtrees[0]);
		PRINT(p, s, "%s", "\n");
		break;
	case RK_EXTERN:
		PRINT(p, s, "%s : ", node->subtrees[0]->text);
		typeToStringParser(p, s, 0, 0, node->subtrees[1]);
		PRINT(p, s, "%s", "\n");
		break;


	}

}

void printTreeDeref(Node *n, int indent, Hashtable *var_types, Region *r) {
	printIndent(indent);
	printf("%s:%d->",n->text, n->nodeType);
        printType(n->coercionType, var_types);
        printf("\n");
	int i;
	for(i=0;i<n->degree;i++) {
		printTreeDeref(n->subtrees[i],indent+1, var_types, r);
	}

}

void printIndent(int n) {
	int i;
	for(i=0;i<n;i++) {
		printf("\t");
	}
}

int eqExprNodeSyntactic(Node *a, Node *b) {
    if(a->nodeType == b->nodeType &&
       strcmp(a->text, b->text) == 0 &&
       a->degree == b->degree) {
        int i;
        for(i=0;i<a->degree;i++) {
            if(!eqExprNodeSyntactic(a->subtrees[i], b->subtrees[i])) {
                return 0;
            }
        }
    }
    return 1;
}
int eqExprNodeSyntacticVarMapping(Node *a, Node *b, Hashtable *varMapping /* from a to b */) {
    char *val;
    if(a->nodeType == TK_VAR && b->nodeType == TK_VAR &&
            (val = (char *)lookupFromHashTable(varMapping, a->text))!=NULL &&
            strcmp(val, b->text) == 0) {
        return 1;
    }
    if(a->nodeType == b->nodeType &&
       strcmp(a->text, b->text) == 0 &&
       a->degree == b->degree) {
        int i;
        for(i=0;i<a->degree;i++) {
            if(!eqExprNodeSyntactic(a->subtrees[i], b->subtrees[i])) {
                return 0;
            }
        }
    }
    return 1;
}

StringList *getVarNamesInExprNode(Node *expr, Region *r) {
    return getVarNamesInExprNodeAux(expr, NULL, r);
}

StringList *getVarNamesInExprNodeAux(Node *expr, StringList *vars, Region *r) {
    int i;
    switch(expr->nodeType) {
        case TK_VAR:
            if(expr->text[0] == '*') {
                StringList *nvars = (StringList*)region_alloc(r, sizeof(StringList));
                nvars->next = vars;
                nvars->str = expr->text;
                return nvars;
            }
            /* non var */
        default:
            for(i =0;i<expr->degree;i++) {
                vars = getVarNamesInExprNodeAux(expr->subtrees[i], vars, r);
            }
            return vars;
    }
}


/************** file/buffer pointer utilities ***************/
void nextChars(Pointer *p, int len) {
	int i;
        for(i=0;i<len;i++) {
		nextChar(p);
	}
}

/**
 * returns -1 if reached eos
 */
int nextChar(Pointer *p) {
    if(p->isFile) {
	int ch = lookAhead(p, 1);
	/*if(ch != -1) { */
		p->p++;
	/*} */
	return ch;
    } else {
        if(p->strbuf[p->strp] == '\0') {
            return -1;
        }
        int ch = p->strbuf[++p->strp];
        if(ch == '\0') {
            ch = -1; /* return -1 for eos */
	}
	return ch;
    }
}

Pointer *newPointer(FILE *fp, char *ruleBaseName) {
        Pointer *e = (Pointer *)malloc(sizeof(Pointer));
        initPointer(e, fp, ruleBaseName);
        return e;
}
Pointer *newPointer2(char* buf) {
        Pointer *e = (Pointer *)malloc(sizeof(Pointer));
        initPointer2(e, buf);

        return e;
}
void deletePointer(Pointer* buf) {
    if(buf->isFile) {
        fclose(buf->fp);
    }
    free(buf->base);
    free(buf);

}

void initPointer(Pointer *p, FILE* fp, char* ruleBaseName /* = NULL */) {
    fseek(fp, 0, SEEK_SET);
    p->fp = fp;
    p->fpos = 0;
    p->len = 0;
    p->p = 0;
    p->isFile = 1;
    p->base = (char *)malloc(strlen(ruleBaseName)+2);
    p->base[0] = 'f';
    strcpy(p->base + 1, ruleBaseName);
}

void initPointer2(Pointer *p, char *buf) {
    p->strbuf = buf;
    p->strlen = strlen(buf);
    p->strp = 0;
    p->isFile = 0;
    p->base = (char *)malloc(strlen(buf)+2);
    p->base[0] = 's';
    strcpy(p->base + 1, buf);
}

void readToBuffer(Pointer *p) {
    if(p->isFile) {
	unsigned int move = (p->len+1)/2;
	move = move > p->p? p->p : move; /* prevent next char from being deleted */
	int startpos = p->len - move;
	int load = POINTER_BUF_SIZE - startpos;
	/* move remaining to the top of the buffer */
	memmove(p->buf,p->buf+move,startpos*sizeof(char));
	/* load from file */
	int count = fread(p->buf+startpos, sizeof(char), load, p->fp);
	p->len = startpos + count;
	p->p -= move;
	p->fpos += move * sizeof(char);
   } else {
   }
}

void seekInFile(Pointer *p, unsigned long x) {
   if(p -> isFile) {
       if(p->fpos < x * sizeof(char) || p->fpos + p->len >= x * sizeof(char)) {
           fseek(p->fp, x * sizeof(char), SEEK_SET);
           clearBuffer(p);
           p->fpos = x * sizeof(char);
           readToBuffer(p);
       } else {
           p->p = x * sizeof(char) - p->fpos;
       }
   } else {
        p->strp = x;
   }
}

void clearBuffer(Pointer *p) {
   if(p->isFile) {
	p->fpos += p->len * sizeof(char);
	p->len = p->p = 0;
   } else {
   }
}

/* assume that n is less then POINTER_BUF_SIZE */
int dupString(Pointer *p, Label * start, int n, char *buf) {
   if(p->isFile) {
	Label curr;
        getFPos(&curr, p);
	seekInFile(p, start->exprloc);
        int len = 0;
        int ch;
        while(len < n && (ch=lookAhead(p, 0)) != -1) {
            buf[len++] = (char) ch;
            nextChar(p);
        }
	buf[len] = '\0';
	seekInFile(p, curr.exprloc);
	return len;
   } else {
	int len = strlen(p->strbuf + start->exprloc);
        len = len > n ? n : len;
        memcpy(buf, p->strbuf + start->exprloc, len * sizeof(char));
        buf[len] = '\0';
        return len;
   }
}

int dupLine(Pointer *p, Label * start, int n, char *buf) {
    Label pos;
    getFPos(&pos, p);
    seekInFile(p, 0);
    int len = 0;
    int i = 0;
    char ch = lookAhead(p, 0);
    while(ch != -1) {
        if(ch=='\n') {
            if(i<start->exprloc)
                len = 0;
            else {
                break;
            }
        } else {
            buf[len] = ch;
            len++;
            if(len == n - 1) {
                break;
            }
        }
        i++;
        ch = nextChar(p);
    }
    buf[len] = '\0';
    seekInFile(p, pos.exprloc);
    return len;
}

void getCoor(Pointer *p, Label * errloc, int coor[2]) {
    Label pos;
    getFPos(&pos, p);
    seekInFile(p, 0);
    coor[0] = coor[1] = 0;
    int i;
    char ch = lookAhead(p, 0);
    for(i =0;i<errloc->exprloc;i++) {
        if(ch=='\n') {
            coor[0]++;
            coor[1] = 0;
        } else {
            coor[1]++;
        }
        ch = nextChar(p);
/*        if(ch == '\r') { skip \r
            ch = nextChar(p);
        } */
    }
    seekInFile(p, pos.exprloc);
}

Label *getFPos(Label *l, Pointer *p) {
    if(p->isFile) {
        l->exprloc = p->fpos/sizeof(char) + p->p;
    } else {
        l->exprloc = p->strp;
    }
    l->base = p->base;
    return l;
}
/* assume that n is less then POINTER_BUF_SIZE/2 and greater than or equal to 0 */
int lookAhead(Pointer *p, unsigned int n) {
	if(p->isFile) {
		if(p->p+n >= p->len) {
			readToBuffer(p);
			if(p->p+n >= p->len) {
				return -1;
			}
		}
		return (int)(p->buf[p->p+n]);
	} else {
		if(n+p->strp >= p->strlen) {
			return -1;
		}
		return (int)p->strbuf[p->strp+n];
	}

}

/* backward compatibility with the previous version */
char *functionParameters(char *e, char *value) {
	int mode=0; /* 0 params 1 string 2 string2 3 escape 4 escape2 */
	int l0 = 0;
	while(*e!=0) {
		*value = *e;
		switch(mode) {
			case 0:

				switch(*e) {
					case '(':
						l0++;
						break;
					case '\"':
						mode = 1;
						break;
					case '\'':
						mode = 2;
						break;
					case ')':
						l0--;
						if(l0==0) {
							value[1]='\0';
							return e+1;
						}
				}
				break;
			case 1:
				switch(*e) {
					case '\\':
						mode = 3;
						break;
					case '\"':
						mode = 0;
				}
				break;
			case 2:
				switch(*e) {
					case '\\':
						mode = 4;
						break;
					case '\'':
						mode = 0;
				}
				break;
			case 3:
			case 4:
				mode -= 2;
		}
		e++; value ++;
	}
	*value=0;
	return e;
}
char *nextStringString(char *e, char *value) {
	int mode=1; /* 1 string 3 escape */
	char* e0=e;
	char* value0=value;
			*value = *e;
	value++;
	e++;
	while(*e!=0) {
		*value = *e;
		switch(mode) {
			case 1:
				switch(*e) {
					case '\\':
					    value--;
						mode = 3;
						break;
					case '\"':
						value[1]='\0';
						trimquotes(value0);
						return e+1;
				}
				break;
			case 3:
				mode -= 2;
		}
		e++; value ++;
	}
	return e0;
}
char *nextString2String(char *e, char *value) {
	int mode=1; /* 1 string 3 escape */
	char* e0=e;
	char* value0 = value;
	*value = *e;
	value ++;
	e++;
	while(*e!=0) {
		*value = *e;
		switch(mode) {
			case 1:
				switch(*e) {
					case '\\':
						value--;
						mode = 3;
						break;
					case '\'':
						value[1]='\0';
						trimquotes(value0);
						return e+1;
				}
				break;
			case 3:
				mode -= 2;
		}
		e++; value ++;
	}
	return e0;
}


char * nextRuleSection(char* buf, char* value) {
	char* e=buf;
	int mode=0; /* 0 section 1 string 2 string2 3 escape 4 escape2 */
	while(*e!=0) {
		*value = *e;
		switch(mode) {
			case 0:

				switch(*e) {
					case '\"':
						mode = 1;
						break;
					case '\'':
						mode = 2;
						break;
					case '|':
						*value='\0';
						return e+1;
				}
				break;
			case 1:
				switch(*e) {
					case '\\':
						mode = 3;
						break;
					case '\"':
						mode = 0;
				}
				break;
			case 2:
				switch(*e) {
					case '\\':
						mode = 4;
						break;
					case '\'':
						mode = 0;
				}
				break;
			case 3:
			case 4:
				mode -= 2;
		}
		e++; value ++;
	}
	*value=0;
	return e;


}

void nextActionArgumentStringBackwardCompatible(Pointer *e, Token *token) {
    skipWhitespace(e);
    char ch = lookAhead(e, 0);
    if (ch==-1) { /* reach the end of stream */
        token->type = TK_EOS;
        strcpy(token->text,"EOS");
    } else {
        ch = lookAhead(e, 0);
        if(ch == '\"') {
            nextStringBase(e, token->text, "\"", 1, '\\', token->vars);
            skipWhitespace(e);
        } else if( ch == '\'') {
            nextStringBase(e, token->text, "\'", 1, '\\', token->vars);
            skipWhitespace(e);
        } else {
            nextStringBase(e, token->text, ",)", 0, '\\', token->vars);
            /* remove trailing ws */
            int l0;
            l0 = strlen(token->text);
            while(isspace(token->text[l0-1])) {
                l0--;
            }
            token->text[l0++]='\0';
        }
        token->type = TK_STRING;
    }
}

PARSER_FUNC_BEGIN(TypingConstraints)
    Hashtable *temp = context->symtable;
    context->symtable = newHashTable2(10, context->region);
    TRY(exec)
        NT(_TypingConstraints);
    FINALLY(exec)
        context->symtable = temp;
    END_TRY(exec)

PARSER_FUNC_END(TypingConstraints)
PARSER_FUNC_BEGIN(Type)
    Hashtable *temp = context->symtable;
    context->symtable = newHashTable2(10, context->region);
    TRY(exec)
        NT2(_Type, 0, 0);
    FINALLY(exec)
        context->symtable = temp;
    END_TRY(exec)
PARSER_FUNC_END(Type)

PARSER_FUNC_BEGIN(FuncType)
    Hashtable *temp = context->symtable;
    context->symtable = newHashTable2(10, context->region);
    TRY(exec)
        NT(_FuncType);
    OR(exec)
        NT2(_Type, 0, 0);
        BUILD_NODE(T_TUPLE, TUPLE, &start, 0, 0);
        SWAP;
        BUILD_NODE(T_CONS, FUNC, &start, 2, 2);
    FINALLY(exec)
        context->symtable = temp;
    END_TRY(exec)
PARSER_FUNC_END(FuncType)

PARSER_FUNC_BEGIN2(_Type, int prec, int lifted)
    int rulegen = 1;
    int arity = 0;
    Node *node = NULL;
    TRY(type)
        ABORT(prec == 1);
        TRY(typeEnd)
            TTEXT_LOOKAHEAD("->");
        OR(typeEnd)
        	TTEXT_LOOKAHEAD("=>");
        OR(typeEnd)
            TTEXT_LOOKAHEAD(")");
        OR(typeEnd)
            TTEXT_LOOKAHEAD(">");
        OR(typeEnd)
            TTYPE_LOOKAHEAD(TK_EOS);
        END_TRY(typeEnd)
    OR(type)
        LOOP_BEGIN(type)
            int cont = 0;
            Label vpos = *FPOS;
            TRY(type)
                    TTYPE(TK_BACKQUOTED); /* irods type */
                    BUILD_NODE(T_IRODS, token->text, &vpos, 0, 0);
            OR(type)
                    TRY(typeVar)
                        TTYPE(TK_INT);
                    OR(typeVar)
                        TTYPE(TK_TEXT);
                        ABORT(!isupper(token->text[0]));
                    END_TRY(typeVar) /* type variable */
                    char *vname = cpStringExt(token->text, context->region);

                    Node *tvar;
                    if ((tvar = (ExprType *) lookupFromHashTable(context->symtable, vname)) == NULL) {
                        TRY(typeVarBound)
                            /* union */
                            NT(TypeSet);
                        OR(typeVarBound)
                            BUILD_NODE(T_VAR, vname, &vpos, 0, 0);
                        END_TRY(typeVarBound)
                        tvar = POP;
                        tvar->value.vid = newTVarId();
                        insertIntoHashTable(context->symtable, vname, tvar);
                    }
                    CASCADE(tvar);
            OR(type)
                    TTEXT("?");
                    CASCADE(newSimpType(T_DYNAMIC, context->region));
            OR(type)
                    TTEXT("integer");
                    CASCADE(newSimpType(T_INT, context->region));
            OR(type)
                    TTEXT("double");
                    CASCADE(newSimpType(T_DOUBLE, context->region));
            OR(type)
                    TTEXT("boolean");
                    CASCADE(newSimpType(T_BOOL, context->region));
            OR(type)
                    TTEXT("time");
                    CASCADE(newSimpType(T_DATETIME, context->region));
            OR(type)
                    TTEXT("string");
                    CASCADE(newSimpType(T_STRING, context->region));
            OR(type)
                    TTEXT("list");
                    NT2(_Type, 1, 0);
                    BUILD_NODE(T_CONS, LIST, &vpos, 1, 1);
                    Node *node = POP;
                    setVararg(node, OPTION_VARARG_ONCE);
                    PUSH(node);
			OR(type)
                    TTEXT("unit");
					BUILD_NODE(T_TUPLE, TUPLE, &vpos, 0, 0);
            OR(type)
                    TTEXT("(");
                    NT2(_Type, 0, 0);
                    TTEXT(")");
            OR(type)
                    TTEXT("<")
                    NT2(_Type, 0, 1);
                    TTEXT(">");
            OR(type)
                    TTEXT("forall");
                    TTYPE(TK_TEXT);
                    ABORT(!isupper(token->text[0]));
                    char *vname = cpStringExt(token->text, context->region);
                    TRY(typeVarBound)
                        TTEXT("in");
                        NT(TypeSet);
                    OR(typeVarBound)
                        BUILD_NODE(T_VAR, vname, &vpos, 0, 0);
                    END_TRY(typeVarBound)
                    Node *tvar = POP;
                    tvar->value.vid = newTVarId();
                    TTEXT(",");
                    insertIntoHashTable(context->symtable, vname, tvar);
                    cont = 1;
            OR(type)
                    TTEXT("f");
                    /* flexible type, non dynamic coercion allowed */
                    NT2(_Type, 1, 0);
                    TRY(ftype)
                    	TTEXT("=>");
                    	NT2(_Type, 1, 0);
                    	BUILD_NODE(T_FIXD, NULL, &start, 2, 2);
                    OR(ftype)
                    	BUILD_NODE(T_FLEX, NULL, &start, 1, 1);
                    END_TRY(ftype);

            OR(type)
                    TTEXT2("output","o");
                    NT2(_Type, 1, 0);
                    node = POP;
                    setIOType(node, IO_TYPE_OUTPUT);
                    PUSH(node);
            OR(type)
                    TTEXT2("input","i");
                    NT2(_Type, 1, 0);
                    node = POP;
                    setIOType(node, IO_TYPE_INPUT | (getIOType(node) & IO_TYPE_OUTPUT));
                    PUSH(node);
            OR(type)
					TTEXT2("expression","e");
                    NT2(_Type, 1, 0);
                    node = POP;
                    setIOType(node, IO_TYPE_EXPRESSION);
                    PUSH(node);
            OR(type)
					TTEXT2("actions","a");
                    NT2(_Type, 1, 0);
                    node = POP;
                    setIOType(node, IO_TYPE_ACTIONS);
                    PUSH(node);
			OR(type)
					TTEXT2("dynamic", "d");
					NT2(_Type, 1, 0);
					node = POP;
					setIOType(node, IO_TYPE_DYNAMIC);
					PUSH(node);
            OR(type)
                    TTEXT("type");
                    CASCADE(newSimpType(T_TYPE, context->region));
                    /* type */
			OR(type)
					TTEXT("set");
					CASCADE(newSimpType(T_TYPE, context->region));
					/* set */
            OR(type)
                    TTYPE(TK_TEXT);
                    char *cons = cpStringExt(token->text, context->region);
                    /* user type */
                    int n = 0;
                    OPTIONAL_BEGIN(argList)
                        TTEXT("(");
                        LOOP_BEGIN(args)
                            NT2(_Type, 1, 0);
                            n++;
                            TRY(delim)
                                TTEXT(",");
                            OR(delim)
                                DONE(args);
                            END_TRY(delim)
                        LOOP_END(args)
                        TTEXT(")")
                    OPTIONAL_END(argList)
                    BUILD_NODE(T_CONS, cons, &start, n, n);
            END_TRY(type)
            if(!cont) {
                arity ++;
                TRY(typeEnd)
                    ABORT(prec!=1);
                    DONE(type);
                OR(typeEnd)
                    TTYPE_LOOKAHEAD(TK_EOS);
                    DONE(type);
                OR(typeEnd)
                    TTEXT("*");
                    TTEXT("->");
                    PUSHBACK;
                    PUSHBACK;
                    DONE(type);
                OR(typeEnd)
                    TTEXT("*");
                OR(typeEnd);
                    DONE(type);
                END_TRY(typeEnd)
            }
        LOOP_END(type)
    END_TRY(type)
    if(arity != 1 || lifted) {
        BUILD_NODE(T_TUPLE, TUPLE, &start, arity, arity);
    }
PARSER_FUNC_END(_Type)

PARSER_FUNC_BEGIN(TypeSet)
    int rulegen = 1;
    TTEXT("{");
    int n = 0;
    int done2 = 0;
    while (!done2 && NO_SYNTAX_ERROR) {
        NT2(_Type, 1, 0);
        n++;
        OPTIONAL_BEGIN(typeVarBoundEnd)
            TTEXT("}");
            done2 = 1;
        OPTIONAL_END(typeVarBoundEnd)
    }
    if(done2) {
        BUILD_NODE(T_VAR, NULL, &start, n, n);
    } else {
        break;
    }
PARSER_FUNC_END(TypeSet)

PARSER_FUNC_BEGIN(_TypingConstraints)
    int rulegen = 1;
    TTEXT("{");
    int n = 0;
    LOOP_BEGIN(tc)
    	Label pos2 = *FPOS;
        NT2(_Type, 0, 0);
        TTEXT("<=");
    	NT2(_Type, 0, 0);
    	BUILD_NODE(TC_LT, "", &pos2, 0, 0); /* node for generating error messages */
    	BUILD_NODE(TC_LT, "<=", &pos2, 3, 3);
        n++;
        TRY(tce)
        	TTEXT(",");
        OR(tce)
            TTEXT("}");
            DONE(tc);
        END_TRY(tce)
    LOOP_END(tc)
    BUILD_NODE(TC_SET, "{}", &start, n, n);
PARSER_FUNC_END(TypingConstraints)

PARSER_FUNC_BEGIN(_FuncType)
    int rulegen = 1;
	int vararg = 0;
    NT2(_Type, 0, 1);
    TRY(vararg)
        TTEXT("*");
        vararg = OPTION_VARARG_STAR;
    OR(vararg)
        TTEXT("+");
        vararg = OPTION_VARARG_PLUS;
	OR(vararg)
		TTEXT("?");
		vararg = OPTION_VARARG_OPTIONAL;
    OR(vararg)
        vararg = OPTION_VARARG_ONCE;
    END_TRY(vararg)
    TTEXT("->");
    NT2(_Type, 0, 0);
    BUILD_NODE(T_CONS, FUNC, &start, 2, 2);
    Node *node = POP;
    setVararg(node, vararg);
    PUSH(node);
PARSER_FUNC_END(_FuncType)

int parseRuleSet(Pointer *e, RuleSet *ruleSet, Env *funcDescIndex, int *errloc, rError_t *errmsg, Region *r) {
    char errbuf[ERR_MSG_LEN];
	Token *token;
    ParserContext *pc = newParserContext(errmsg, r);

        int ret = 1;
        /* parser variables */
        int backwardCompatible = 0;

        while(ret == 1) {
        	pc->nodeStackTop = 0;
        	pc->stackTopStackTop = 0;
            token = nextTokenRuleGen(e, pc, 1);
            switch(token->type) {
                case N_ERROR:
                    return -1;
                case TK_EOS:
                    ret = 0;
                    continue;
                case TK_MISC_OP:
                case TK_TEXT:
                    if(token->text[0] == '#') {
                        skipComments(e);
                        continue;
                    } else if(token->text[0] == '@') { /* directive */
                    	token = nextTokenRuleGen(e, pc, 1);
                        if(strcmp(token->text, "backwardCompatible")==0) {
                        	token = nextTokenRuleGen(e, pc, 1);
                            if(token->type == TK_TEXT) {
                            if(strcmp(token->text, "true")==0) {
                                backwardCompatible = 1;
                            } else if(strcmp(token->text, "false")==0) {
                                backwardCompatible = 0;
                            } else {
                                /* todo error handling */
                            }
                            } else {
                            	/* todo error handling */
                            }
                        } else if(strcmp(token->text, "include")==0) {
                        	token = nextTokenRuleGen(e, pc, 1);
                        	if(token->type == TK_TEXT || token->type == TK_STRING) {
                        		CASCASE_NON_ZERO(readRuleSetFromFile(token->text, ruleSet, funcDescIndex, errloc, errmsg, r));
                        	} else {
                        		/* todo error handling */
                        	}
                        } else {
                            /* todo error handling */
                        }
                        continue;
                    }
                    break;
                default:
                    break;
            }
            pushback(e, token, pc);

            Node *node = parseRuleRuleGen(e, backwardCompatible, pc, errmsg, r);
            if(node==NULL) {
                addRErrorMsg(errmsg, OUT_OF_MEMORY, "parseRuleSet: out of memory.");
                return -1;
            } else if(node->nodeType == N_ERROR) {
                    *errloc = node->expr;
                    generateErrMsg("parseRuleSet: error parsing rule.", *errloc, e->base, errbuf);
                    addRErrorMsg(errmsg, PARSER_ERROR, errbuf);
                    /* skip the current line and try to parse the rule from the next line */
                    skipComments(e);
                    return -1;
            } else {
                int n = node->degree;
                Node **nodes = node->subtrees;
                RuleType rk;
/*                if(strcmp(node->text, "UNPARSED") == 0) {
                	pushRule(ruleSet, newRuleDesc(RK_UNPARSED, nodes[0], r));
                } else */ if(strcmp(node->text, "INDUCT") == 0) {
                    int k;
                    pushRule(ruleSet, newRuleDesc(RK_DATA, nodes[0], r));
                    for(k=1;k<n;k++) {
                        if(lookupFromEnv(funcDescIndex, nodes[k]->subtrees[0]->text)!=NULL) {
                            generateErrMsg("parseRuleSet: redefinition of constructor.", nodes[k]->subtrees[0]->expr, nodes[k]->subtrees[0]->base, errbuf);
                            addRErrorMsg(errmsg, TYPE_ERROR, errbuf);
                            return -1;
                        }
                        insertIntoHashTable(funcDescIndex->current, nodes[k]->subtrees[0]->text, newConstructorFD2(nodes[k]->subtrees[1], r));
                        pushRule(ruleSet, newRuleDesc(RK_CONSTRUCTOR, nodes[k], r));
                    }
                } else if(strcmp(node->text, "CONSTR") == 0) {
					if(lookupFromEnv(funcDescIndex, nodes[0]->subtrees[0]->text)!=NULL) {
						generateErrMsg("parseRuleSet: redefinition of constructor.", nodes[0]->subtrees[0]->expr, nodes[0]->subtrees[0]->base, errbuf);
						addRErrorMsg(errmsg, TYPE_ERROR, errbuf);
						return -1;
					}
					insertIntoHashTable(funcDescIndex->current, nodes[0]->subtrees[0]->text, newConstructorFD2(nodes[0]->subtrees[1], r));
					pushRule(ruleSet, newRuleDesc(RK_CONSTRUCTOR, nodes[0], r));
                } else if(strcmp(node->text, "EXTERN") == 0) {
                	FunctionDesc *fd;
					if((fd = (FunctionDesc *) lookupFromEnv(funcDescIndex, nodes[0]->subtrees[0]->text))!=NULL) {
						generateErrMsg("parseRuleSet: redefinition of function.", nodes[0]->subtrees[0]->expr, nodes[0]->subtrees[0]->base, errbuf);
						addRErrorMsg(errmsg, TYPE_ERROR, errbuf);
						return -1;
					}
					insertIntoHashTable(funcDescIndex->current, nodes[0]->subtrees[0]->text, newExternalFD(nodes[0]->subtrees[1], r));
					pushRule(ruleSet,  newRuleDesc(RK_EXTERN, nodes[0], r));
                } else {
                    if(strcmp(node->text, "REL")==0) {
                        rk = RK_REL;
                    } else if(strcmp(node->text, "FUNC")==0) {
                        rk = RK_FUNC;
                    }
                    int k;
                    for(k=0;k<n;k++) {
                        Node *node = nodes[k];
                        pushRule(ruleSet, newRuleDesc(rk, node, r));
            /*        printf("%s\n", node->subtrees[0]->text);
                    printTree(node, 0); */
                    }
                }
            }
		}
        deleteParserContext(pc);
		return 0;
}

/*
 * parse the string for an ExprType
 * supported types:
 * ? dynamic
 * i integer
 * b boolean
 * d double
 * t time
 * s string
 * list <type> list
 * f <type> flexible
 * <var> ({ <type> ... <type> })? variable
 * <type> * ... * <type> product
 * < <type> * ... * <type> > lifted product
 * `irods PI` irods
 *
 * <type> (*|+|?)? -> <type> function
 * forall <var> (in { <type> ... <type> })?, <type> universal
 */
Node* parseFuncTypeFromString(char *string, Region *r) {
    Pointer *p = newPointer2(string);
    ParserContext *pc = newParserContext(NULL, r);
    nextRuleGenFuncType(p, pc);
    Node *exprType = pc->nodeStack[0];
    deleteParserContext(pc);
    deletePointer(p);
    return exprType;
}
Node* parseTypingConstraintsFromString(char *string, Region *r) {
    Pointer *p = newPointer2(string);
    ParserContext *pc = newParserContext(NULL, r);
    nextRuleGenTypingConstraints(p, pc);
    Node *exprType = pc->nodeStack[0];
    /*char buf[ERR_MSG_LEN];
    errMsgToString(pc->errmsg, buf, ERR_MSG_LEN);
    printf("%s", buf);*/
    deleteParserContext(pc);
    deletePointer(p);
    return exprType;
}
Node *parseRuleRuleGen(Pointer *expr, int backwardCompatible, ParserContext *pc, rError_t *errmsg, Region *r) {
    nextRuleGenRule(expr, pc, backwardCompatible);
    Node *rulePackNode = pc->nodeStack[0];
    if(pc->error) {
        if(pc->errnode!=NULL) {
            rulePackNode =pc->errnode;
        } else {
            rulePackNode =createErrorNode("parser error", &pc->errloc, r);
        }
    }
    return rulePackNode;
}
Node *parseTermRuleGen(Pointer *expr, int rulegen, ParserContext *pc, rError_t *errmsg, Region *r) {
    nextRuleGenTerm(expr, pc, rulegen, 0);
    Node *rulePackNode = pc->nodeStack[0];
        if(pc->error) {
        if(pc->errnode!=NULL) {
            rulePackNode =pc->errnode;
        } else {
            rulePackNode =createErrorNode("parser error", &pc->errloc, r);
        }
    }

    return rulePackNode;

}
char* typeName_Res(Res *s) {
    return typeName_ExprType(s->exprType);
}

char* typeName_ExprType(ExprType *s) {
        switch(s->nodeType) {
            case T_IRODS:
                return s->text;
            default:
                return typeName_NodeType(s->nodeType);
        }
}

char* typeName_Parser(NodeType s) {
        switch(s) {
            case T_IRODS:
                return "IRODS";
            case T_VAR:
                return "VAR";
            case T_DYNAMIC:
                return "?";
            case T_CONS:
                return "CONS";
            case T_FLEX:
                return "f";
            case T_FIXD:
                return "f";
            case T_BOOL:
                return "boolean";
            case T_INT:
                return "integer";
            case T_DOUBLE:
                return "double";
            case T_STRING:
                return "string";
            case T_ERROR:
                return "ERROR";
            case T_DATETIME:
                return "time";
            case T_TYPE:
                return "set";
            case T_TUPLE:
            	return "TUPLE";
            default:
                return "OTHER";
        }
}

char* typeName_NodeType(NodeType s) {
        switch(s) {
            case T_IRODS:
                return "IRODS";
            case T_VAR:
                return "VAR";
            case T_DYNAMIC:
                return "DYNAMIC";
            case T_CONS:
                return "CONS";
            case T_FLEX:
                return "FLEX";
            case T_FIXD:
                return "FIXD";
            case T_BOOL:
                return "BOOLEAN";
            case T_INT:
                return "INTEGER";
            case T_DOUBLE:
                return "DOUBLE";
            case T_STRING:
                return "STRING";
            case T_ERROR:
                return "ERROR";
            case T_DATETIME:
                return "DATETIME";
            case T_TYPE:
                return "TYPE";
            case T_TUPLE:
            	return "TUPLE";
            default:
                return "OTHER";
        }
}

void generateErrMsgFromFile(char *msg, long errloc, char *ruleBaseName, char* ruleBasePath, char errbuf[ERR_MSG_LEN]) {
    FILE *fp = fopen(ruleBasePath, "r");
    Pointer *e = newPointer(fp, ruleBaseName);
    Label l;
    l.base = NULL;
    l.exprloc = errloc;
    generateErrMsgFromPointer(msg, &l, e, errbuf);
    deletePointer(e);
}

void generateErrMsgFromSource(char *msg, long errloc, char *src, char errbuf[ERR_MSG_LEN]) {
    Pointer *e = newPointer2(src);
    Label l;
    l.base = NULL;
    l.exprloc = errloc;
    generateErrMsgFromPointer(msg, &l, e, errbuf);
    deletePointer(e);
}
void generateErrMsgFromPointer(char *msg, Label *l, Pointer *e, char errbuf[ERR_MSG_LEN]) {
    char buf[ERR_MSG_LEN];
    dupLine(e, l, ERR_MSG_LEN, buf);
    int len = strlen(buf);
    int coor[2];
    getCoor(e, l, coor);
    int i;
    if(len < ERR_MSG_LEN - 1) {
        buf[len++] = '\n';
    }
    for(i=0;i<coor[1];i++) {
        if(len >= ERR_MSG_LEN - 1) {
            break;
        }
        buf[len++] = buf[i] == '\t'?'\t':' ';
    }
    if(len < ERR_MSG_LEN - 2) {
        buf[len++] = '^';
    }
    buf[len++] = '\0';
    if(e->isFile)
    snprintf(errbuf, ERR_MSG_LEN,
            "%s\nline %d, col %d, rule base %s\n%s\n", msg, coor[0], coor[1], e->base+1, buf);
    else
    snprintf(errbuf, ERR_MSG_LEN,
            "%s\nline %d, col %d\n%s\n", msg, coor[0], coor[1], buf);

}
void generateAndAddErrMsg(char *msg, Node *node, int errcode, rError_t *errmsg) {
	char errmsgBuf[ERR_MSG_LEN];
	generateErrMsg(msg, node->expr, node->base, errmsgBuf);
	addRErrorMsg(errmsg, errcode, errmsgBuf);
}
char *generateErrMsg(char *msg, long errloc, char *ruleBaseName, char errmsg[ERR_MSG_LEN]) {
    char ruleBasePath[MAX_NAME_LEN];
    switch(ruleBaseName[0]) {
        case 's': // source
            generateErrMsgFromSource(msg, errloc, ruleBaseName + 1, errmsg);
            return errmsg;
        case 'f': // file
            getRuleBasePath(ruleBaseName + 1, ruleBasePath);
            generateErrMsgFromFile(msg, errloc, ruleBaseName + 1, ruleBasePath, errmsg);
            return errmsg;
        default:
            snprintf(errmsg, ERR_MSG_LEN, "<unknown source type>");
            return errmsg;
    }
}
