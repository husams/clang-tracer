grammar TracerCommands;

line : '%' command EOF;
command : server | attach | processes | sessions | use | status | detach
        | modules | symbols | probe | interceptor | events | collect | record
        | cpp | shell | complete | help | quit;
server : 'server' ('start' serverOption* | 'connect' value | 'status' | 'stop');
serverOption : ('--executable' | '--agent') value;
attach : 'attach' UINT?;
processes : 'processes' ('--filter' value)?;
sessions : 'sessions';
use : 'use' value;
status : 'status';
detach : 'detach' value?;
modules : 'modules';
symbols : 'symbols' symbolOption*;
symbolOption : ('--filter' | '--module' | '--page') value | '--limit' UINT;
probe : 'probe' ('prepare' value prepareOption+
      | 'resolve' value resolveOption*
      | 'inject' value '--callback' value deployOption*
      | 'validate' value '--callback' value resolveOption*
      | 'remove' UINT) | 'probes';
prepareOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--output') value
              | METADATA_ONLY;
resolveOption : ('--include' | '--compdb' | '--module' | '--compile-option'
                | '--prepared' | '--prepared-mode') value;
deployOption : resolveOption | '--stack-depth' UINT | '--print-arguments' | '--print-result';
interceptor : 'interceptor' ('load' value '--as' value | 'edit' value | 'show' value
            | 'upload' value '--target' value deployOption*);
events : 'events' (streamOption*
       | 'list' value ('--limit' UINT)?
       | 'show' value '--index' UINT
       | 'calls' value | 'stats' value | 'clear' value
       | 'open' value '--as' value
       | 'export' value '--format' 'json' '--output' value);
collect : 'collect' streamOption*;
record : 'record' value streamOption*;
streamOption : ('--count' | '--timeout' | '--after' | '--probe') UINT
             | '--phase' ('PRE' | 'POST')
             | ('--as' | '--name' | '--session') value | '--all';
cpp : 'cpp' ('load' value | 'reset'
            | ('import' | 'generate') value (('--type' | '--output') value)*
            | 'options' ('add' (value | SHELL)+ | 'clear'
                        | 'compdb' value '--file' value)?);
shell : 'shell' value;
complete : 'complete' value;
help : 'help' value?;
quit : 'quit' | 'exit';
value : STRING | WORD | UINT | keyword;
keyword : 'server' | 'start' | 'connect' | 'status' | 'stop' | 'attach'
        | 'processes' | 'sessions' | 'use' | 'detach' | 'modules' | 'symbols'
        | 'probe' | 'prepare' | 'resolve' | 'inject' | 'validate' | 'remove' | 'probes'
        | 'interceptor' | 'load' | 'edit' | 'show' | 'upload' | 'events'
        | 'list' | 'calls' | 'stats' | 'clear' | 'open' | 'export' | 'json'
        | 'collect' | 'record' | 'PRE' | 'POST' | 'cpp' | 'reset' | 'import'
        | 'generate' | 'options' | 'add' | 'compdb' | 'shell'
        | 'complete' | 'help' | 'quit' | 'exit';
UINT : [0-9]+;
STRING : '"' ('\\' ["\\nrt] | ~["\\\r\n])* '"'
       | '\'' ('\\' ['\\nrt] | ~['\\\r\n])* '\'';
SHELL : '`' ~[`\r\n]+ '`';
METADATA_ONLY : '--metadata-only';
WORD : ~[ \t\r\n"'\\%`]+;
WS : [ \t\r\n]+ -> skip;
