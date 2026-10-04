# CAT compatibility contracts

`compatibility.csv` carries all 419 approved descriptor contracts and concrete
case references into `requests.json`. Each fixture identifies a command form,
request bytes, source, explicit precondition, expected response/mutation when
materialized, and separate fixture/execution status. Empty expectedReply means
successful silence; JSON null means no response has been invented.

There are 752 request records, 566 materialized response
contracts and 186 pending model response contracts.
The pending cases retain exact descriptor widths, source ranges, target APIs,
read/set semantic requirements and stable case IDs. Zero numeric payloads in
pending cases are parser structure examples, not claims of valid semantic ranges
or successful mutation. Tasks 5-7 must replace them with family-owned concrete
production model setup and expected bytes before advertising handler support.

Task 5 executes 158 records across 99 owning descriptors through the actual service/router
and native models in `tst_cat_rx_commands::fixtureExecution`, with source-valid
requests and `familyModelSetup`/`expectedState` assertions. Their execution status
is `executed-task-5`; the remaining 594 records are `not-executed`. The catalogue
and parser tests check structure/formatting only; their counts are not execution
evidence. Task 7 must execute all production families. Never count catalogue/fixture counts as
functional coverage. Source-inert replies preserve source constants only where
verified; inactive/unavailable fixtures always reject without mutation. Equal
get/set widths follow GetFirst for ZZZM/ZZZV, SetFirst for unavailable ZZJS.

Static examples cite CATCommands.cs:223-237,721-731,4881-4900,1573-1603,
6447-6470,6881-6905,3011-3092,3444-3447; setup.cs:5720-5728 and
console.cs:16104-16118 [v2.10.3.15]. FA/FB frequency formatting cites
CATCommands.cs:2653-2753,9221-9261; EQ uses 2508-2587; AI uses 1223-1250;
ID uses 291-313; ZZEM uses 2590-2609. Extended formatter echoes request suffix
(CATParser.cs:1535-1547). GUID replies use the approved canonical TCP frame
correction recorded in the matrix; source serial formatting duplicates the
GUID. Header notices are preserved byte-for-byte in HEADERS.md.

ZZMX source dispatch is present at CATParser.cs:965-967 (`case"ZZMX":`). Its
previous missing-dispatch claim was an extractor false negative. The outcome
remains Unavailable because NereusSDR lacks the full memory restoration API.
