grammar TracerCommands;

line : '%' command EOF;
command : server | attach | processes | sessions | use | status | detach
        | modules | threads | symbols | probe | interceptor | events | collect | record
        | cpp | shell | complete | type | help | quit | bulkTrace | memory | probeEntry | probeReload;
server : 'server' ('start' serverOption* | 'connect' value | 'status' | 'stop');
serverOption : ('--executable' | '--agent') value | contextEventBytes;
attach : 'attach' (UINT ('--start-time' UINT)?)?;
processes : 'processes' ('--filter' value)?;
sessions : 'sessions';
use : 'use' value;
status : 'status';
detach : 'detach' value?;
modules : 'modules' '--details'?;
threads : 'threads';
symbols : 'symbols' symbolOption*;
symbolOption : ('--filter' | '--module' | '--page' | '--address' | '--mapping') value
             | '--limit' UINT | '--addresses';
probe : 'probe' ('prepare' value prepareOption+
      | 'resolve' value (resolveOption | cwdOption)*
      | 'inject' value '--callback' value (deployOption | cwdOption)*
      | 'validate' value '--callback' value (resolveOption | cwdOption)*
      | 'remove' UINT cwdOption?) | 'probes' cwdOption?;
prepareOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--output') value
              | SIGNATURE value | MEMBER | METADATA_ONLY | extraCompileOption | cwdOption;
resolveOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--compile-option'
                | '--prepared' | '--prepared-mode') value
              | extraCompileOption;
deployOption : resolveOption | '--stack-depth' (UINT | WORD)
             | '--print-arguments' | '--print-result' | stackFilter;
interceptor : 'interceptor' ('load' value '--as' value | 'edit' value | 'show' value
      | 'validate' value '--target' value resolveOption*
            | 'upload' value '--target' value deployOption*);
events : 'events' (streamOption*
       | 'list' value ('--limit' UINT)?
       | 'show' value '--index' UINT
       | STACK value '--index' UINT
       | 'calls' value | 'stats' value | 'clear' value
       | 'open' value '--as' value
       | 'export' value '--format' 'json' '--output' value);
collect : 'collect' streamOption*;
record : 'record' value (streamOption | SQLITE)*;
streamOption : ('--count' | '--timeout' | '--after' | '--probe') UINT
             | '--phase' ('PRE' | 'POST')
             | ('--as' | '--name' | '--session') value | '--all';
cpp : 'cpp' ('load' value | 'reset'
            | ('import' | 'generate') value (('--type' | '--output') value)*
            | 'options' ('add' (value | SHELL)+ | 'clear'
                        | 'compdb' value '--file' value)?);
shell : 'shell' value;
complete : 'complete' value;
type : 'type' value;
help : 'help' ('%'? value+)?;
quit : 'quit' | 'exit';
bulkTrace : 'trace' ('preview' bulkTraceOption+
          | 'apply' '--manifest' value ('--callback' value)?
          | 'remove' value);
bulkTraceOption : ('--module' | '--include' | '--exclude' | '--exact' | '--manifest' | '--save') value;
value : STRING | WORD | UINT | keyword;
keyword : 'server' | 'start' | 'connect' | 'status' | 'stop' | 'attach'
        | 'processes' | 'sessions' | 'use' | 'detach' | 'modules' | 'threads' | 'symbols'
        | 'probe' | 'prepare' | 'resolve' | 'inject' | 'validate' | 'remove' | 'probes'
        | 'interceptor' | 'load' | 'edit' | 'show' | 'upload' | 'events'
        | 'list' | 'calls' | 'stats' | 'clear' | 'open' | 'export' | 'json'
        | 'collect' | 'record' | 'PRE' | 'POST' | 'cpp' | 'reset' | 'import'
        | 'generate' | 'options' | 'add' | 'compdb' | 'shell'
        | 'complete' | 'type' | 'help' | 'quit' | 'exit' | 'trace' | 'preview' | 'apply'
        | 'memory' | 'maps' | 'read' | 'scan' | 'entry' | 'reload' | STACK;
memory : 'memory' ('maps' | 'read' value '--mapping' value
                 ('--count' UINT | '--type' value)
                 | 'scan' value '--mapping' value '--count' UINT
                   '--pattern' value ('--limit' UINT)?);
probeEntry : 'probe' 'entry' (value '--mapping' value ('--module' value)?
           | '--module' value '--elf-address' value '--abi' value '--mapping' value) cwdOption?;
probeReload : 'probe' 'reload' UINT '--callback' value cwdOption?;
stackFilter : '--stack-application-only' value;
contextEventBytes : ('--context-event-bytes' | '--context-transport-memory-bytes') UINT;
extraCompileOption : '--extra-arg' value | '--extra-args' (value | SHELL)+;
cwdOption : CWD value;
UINT : [0-9]+;
STRING : '"' ('\\' ["\\nrt] | ~["\\\r\n])* '"'
       | '\'' ('\\' ['\\nrt] | ~['\\\r\n])* '\'';
SHELL : '`' ~[`\r\n]+ '`';
METADATA_ONLY : '--metadata-only';
SQLITE : '--sqlite';
STACK : 'stack';
CWD : '--cwd';
SIGNATURE : '--signature';
MEMBER : '--member';
WORD : ~[ \t\r\n"'\\%`]+;
WS : [ \t\r\n]+ -> skip;
