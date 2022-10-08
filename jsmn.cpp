#include  "jsmn.h"

jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, List<jsmntok_t> *tokens) {
  parser->toknext++;

  auto tok = tokens->append();
  tok->start = tok->end = -1;
  tok->size = 0;
  return tok;
}

void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type, const int start, const int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

int jsmn_parse_primitive(jsmn_parser *parser, const char *js, const size_t len, List<jsmntok_t> *tokens) {
  jsmntok_t *token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    switch (js[parser->pos]) {
    case ':':
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }

found:
  token = jsmn_alloc_token(parser, tokens);
  if (token == NULL) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
  parser->pos--;
  return 0;
}

int jsmn_parse_string(jsmn_parser *parser, const char *js, const size_t len, List<jsmntok_t> *tokens) {
  jsmntok_t *token;

  int start = parser->pos;

  parser->pos++;

  /* Skip starting quote */
  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') {
      token = jsmn_alloc_token(parser, tokens);
      if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) {
      int i;
      parser->pos++;
      switch (js[parser->pos]) {
      /* Allowed escaped symbols */
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      /* Allows escaped symbol \uXXXX */
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
          /* If it isn't a hex character we have an error */
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
                (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      /* Unexpected symbol */
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len, List<jsmntok_t> *tokens) {
  int r;
  int i;
  jsmntok_t *token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) {
    case '{':
    case '[':
      count++;
      token = jsmn_alloc_token(parser, tokens);
      if (token == NULL) return JSMN_ERROR_NOMEM;
      if (parser->toksuper != -1) {
        jsmntok_t *t = &tokens->items[parser->toksuper];
        t->size++;
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
      for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens->items[i];
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) return JSMN_ERROR_INVAL;
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      /* Error if unmatched closing bracket */
      if (i == -1) return JSMN_ERROR_INVAL;
      for (; i >= 0; i--) {
        token = &tokens->items[i];
        if (token->start != -1 && token->end == -1) {
          parser->toksuper = i;
          break;
        }
      }
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens);
      if (r < 0) return r;
      count++;
      if (parser->toksuper != -1 && tokens != NULL)
        tokens->items[parser->toksuper].size++;
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 && tokens->items[parser->toksuper].type != JSMN_ARRAY && tokens->items[parser->toksuper].type != JSMN_OBJECT) {
        for (i = parser->toknext - 1; i >= 0; i--) {
          if (tokens->items[i].type == JSMN_ARRAY || tokens->items[i].type == JSMN_OBJECT) {
            if (tokens->items[i].start != -1 && tokens->items[i].end == -1) {
              parser->toksuper = i;
              break;
            }
          }
        }
      }
      break;
    default:
      r = jsmn_parse_primitive(parser, js, len, tokens);
      if (r < 0) return r;
      count++;
      if (parser->toksuper != -1 && tokens != NULL)
        tokens->items[parser->toksuper].size++;
      break;
    }

  }

  if (tokens != NULL)
    for (i = parser->toknext - 1; i >= 0; i--)
      /* Unmatched opened object or array */
      if (tokens->items[i].start != -1 && tokens->items[i].end == -1)
          return JSMN_ERROR_PART;

  return count;
}

void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}
