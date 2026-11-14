# no-port-check: NereusSDR-original.
# 2026-10-30: bounded real-service visual fixture, J.J. Boyd / KG4VCF, Codex.
"""Six virtual stations through the real service, HTTP and WebSocket paths.

This is a local test fixture, never production presence or authentication.
Run only when the controller admits UI verification. Reuses the established
loopback proxy and real one-time sign-in links. Close stdin to stop it.
"""
import asyncio
import sys
from urllib.parse import parse_qs
import serve


class FrequencyWorld(serve.World):
    def build(self):
        same=serve.RoomPlan("80m",3965000,"LSB","talking")
        edge=serve.RoomPlan("80m",3960000,"LSB","quiet")
        outside=serve.RoomPlan("80m",3970001,"LSB","quiet")
        self.rooms=[same,edge,outside]
        self.kg=self.add_sim(same,"reader","K4TST","EM85")
        a=self.add_sim(same,"reader","W1TST","FM05")
        b=self.add_sim(same,"reader","N2TST","FN31")
        missing=self.add_sim(same,"reader","N3TST","EM85")
        missing.grid=None
        self.add_sim(edge,"quiet","N4TST","EM73")
        self.w8=self.add_sim(outside,"quiet","N5TST","EN52")
        for sim in self.sims:
            sim.offset=0
            sim.period=1000
            sim.next=0
            sim.dbm=-83 if sim is a else -97 if sim is b else -90
            sim.presence()
        self.clock.advance(10000)
        self.kg.key(True)
        self.step(1000)
        self.kg.post("Who is hearing this station?","fixture-first")
        a.post("Readable here. **Good copy** at https://example.org/radio","fixture-reply")
        self.kg.key(False)
        self.clock.advance(3000)
        self.kg.key(True)
        self.step(1000)
        self.catch_up()
        self.facts.update({"rooms":3,"stations":6,"owner":"K4TST"})

    async def control(self,target):
        path,_,query=target.partition("?")
        if path=="/__test/messages":
            for call,text in (("W1TST","K4TST, a real nearby mention for exposure acceptance."),
                              ("N4TST","Inclusive edge message at 3.960000 MHz."),
                              ("N5TST","Outside message at 3.970001 MHz.")):
                sim=next(s for s in self.sims if s.call==call)
                sim.post(text,"acceptance-"+call+"-"+str(self.clock.wall_ms()))
            return 200,{"messages":"sent through actual station service actions"}
        if path=="/__test/scene":
            state=parse_qs(query).get("state",["known"])[0]
            with self.service.event():
                for sim in self.sims:
                    if sim.room is self.kg.room:
                        sim.role="reader"
                        sim.away=state=="away" and sim is self.kg
                        sim.key(sim is self.kg and state in ("known","away","double") or sim.call=="W1TST" and state=="double")
                if state in ("unknown","quiet"):
                    self.clock.advance(3000)
                if state=="unknown":
                    reporter=next(s for s in self.sims if s.call=="W1TST")
                    reporter.role="hearer"
                    reporter.frame(self.clock.wall_ms())
            return 200,{"state":state}
        return await super().control(target)


if __name__=="__main__":
    serve.World=FrequencyWorld
    sys.exit(asyncio.run(serve.main()))
