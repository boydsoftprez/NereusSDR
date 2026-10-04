# CAT compatibility contracts

`compatibility.csv` carries all 419 approved descriptor contracts and concrete
case references into `requests.json`. Each fixture identifies a command form,
request bytes, source, explicit precondition, expected response/mutation when
materialized, and separate fixture/execution status. Empty expectedReply means
successful silence; JSON null means no response has been invented.

There are 783 current request records: the original 752, Task6's 24 literal
buffer regressions, and seven Task7 validation/setup regressions. All 783 have
concrete expected replies and zero remain pending. `tst_cat_coverage` executes
every current record through `CatService::processFrame`, actual models, native
family setup/effect assertions, and no-mutation snapshots. Catalogue/parser
formatting checks remain separate from production execution evidence.

The historical owning-family `executionStatus` labels remain 158 Task5, 174
Task6, 305 Task7, and 146 `not-executed` (140 inactive records and six router
records). The separate `productionCoverageStatus: executed-task-7-production`
records that the combined production coverage target executes all 783, including
those inactive/router cases. Their owning-family labels do not claim an earlier
execution. Prior accepted family harnesses are reused for their native setup and
effect assertions; Task7 supplies the remaining TX/global setup.

The exact production registry matches all 349 active catalogue descriptors and
all 419 CSV rows. All 70 inactive descriptors remain unregistered. Mapping totals
are Faithful15, Adapted163, SourceInert42, Unavailable129 and Inactive70. Explicit
unavailable commands refuse before mutation for their mapped missing native
capabilities. Source-inert values retain only source-proven constants. Equal
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
