grammar TracerCommands;

line : '%' command EOF;
command : server | attach | processes | sessions | use | status | detach
        | modules | symbols | probe | interceptor | events | collect | record
        | cpp | help | quit;
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
probe : 'probe' ('resolve' value resolveOption*
      | 'inject' value '--callback' value deployOption*
      | 'validate' value '--callback' value resolveOption*
      | 'remove' UINT) | 'probes';
resolveOption : ('--include' | '--compdb' | '--module' | '--compile-option') value;
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
cpp : 'cpp' ('load' value | 'reset');
help : 'help' value?;
quit : 'quit' | 'exit';
value : STRING | WORD | UINT | keyword;
keyword : 'server' | 'start' | 'connect' | 'status' | 'stop' | 'attach'
        | 'processes' | 'sessions' | 'use' | 'detach' | 'modules' | 'symbols'
        | 'probe' | 'resolve' | 'inject' | 'validate' | 'remove' | 'probes'
        | 'interceptor' | 'load' | 'edit' | 'show' | 'upload' | 'events'
        | 'list' | 'calls' | 'stats' | 'clear' | 'open' | 'export' | 'json'
        | 'collect' | 'record' | 'PRE' | 'POST' | 'cpp' | 'reset' | 'help' | 'quit' | 'exit';
UINT : [0-9]+;
STRING : '"' ('\\' ["\\nrt] | ~["\\\r\n])* '"'
       | '\'' ('\\' ['\\nrt] | ~['\\\r\n])* '\'';
WORD : ~[ \t\r\n"'\\%]+;
WS : [ \t\r\n]+ -> skip;
