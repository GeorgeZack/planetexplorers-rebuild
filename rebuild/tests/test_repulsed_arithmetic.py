#!/usr/bin/env python3
"""Compare compiled C requests with an independent double Hermite oracle."""
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


def f32(value):
    return struct.unpack('f',struct.pack('f',value))[0]


def hermite(curve,t):
    keys=curve['m_Curve']
    if t<=keys[0]['time']: return keys[0]['value']
    if t>=keys[-1]['time']: return keys[-1]['value']
    a,b=next((a,b) for a,b in zip(keys,keys[1:]) if a['time']<=t<b['time'])
    duration=b['time']-a['time']; u=(t-a['time'])/duration
    # Hermite basis in double precision, independent of the C coefficient
    # expansion/SSE rounding order. This is a tolerance check, not engine parity.
    return ((2*u**3-3*u**2+1)*a['value']+(u**3-2*u**2+u)*duration*a['outSlope']
            +(-2*u**3+3*u**2)*b['value']+(u**3-u**2)*duration*b['inSlope'])


class RepulsedArithmetic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.curves=json.loads((ROOT/'generated/pe_repulsed_source.json').read_text())['binding']['curves']
        cls.directory=tempfile.TemporaryDirectory(prefix='repulsed-arithmetic-')
        cls.addClassCleanup(cls.directory.cleanup)
        stage=Path(cls.directory.name); source=stage/'probe.c'; cls.binary=stage/'probe'
        source.write_text('''#include "pe_repulsed.h"
#include <assert.h>
#include <stdio.h>
int main(void){float force,phase;
    while(scanf("%f %f",&force,&phase)==2){
        PeRepulsedProfile p;assert(pe_repulsed_profile(force,&p));
        PeRepulsed a={0};PeRepulsedUpdate out;
        PeRepulsedStart in={.force=force,.projected_direction={0,0,1}};
        assert(pe_repulsed_begin(&a,in,&out));a.elapsed=a.move_time*phase;
        PeRepulsedTick tick={.context_present=true,.human_phy_present=true};
        assert(pe_repulsed_step(&a,tick,&out));
        printf("%.9g %.9g %.9g %.9g %.9g\\n",p.velocity,p.move_time,p.stop_time,p.wentfly_time,
               out.apply_move_request?out.move_request.z:0);
    }return 0;}
''')
        library=ROOT/'build/obj/core/libpe_core.a'
        if not library.is_file(): raise AssertionError('Build libpe_core.a before this test')
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Wpedantic',
                        '-I'+str(ROOT/'src'),str(source),str(library),'-lm','-o',str(cls.binary)],check=True,capture_output=True,text=True)

    def samples(self,pairs):
        values=[(f32(force),f32(phase)) for force,phase in pairs]
        data=''.join(f'{force:.9g} {phase:.9g}\n' for force,phase in values)
        result=subprocess.run([str(self.binary)],input=data,text=True,capture_output=True,check=True)
        rows=[list(map(float,row.split())) for row in result.stdout.splitlines()]
        self.assertEqual(len(rows),len(values)); return zip(values,rows)

    def near(self,actual,expected):
        self.assertAlmostEqual(actual,expected,delta=1e-5+abs(expected)*2e-5)

    def test_force_curve_boundaries_and_narrow_wentfly_window(self):
        forces=set(range(0,2501,25))|{700.04150390625,749.9514770507812,1e10}
        for curve in self.curves.values():
            for key in curve['m_Curve']:
                t=key['time']
                if t>1: forces.update((max(0,t-.001),t,t+.001))
        forces.update(700+i*.0005 for i in range(85))
        names=('m_ForceToVelocity','m_ForceToMoveTime','m_ApplyMoveStopTime','m_WentflyTimeCurve')
        for (force,_),row in self.samples([(force,0) for force in sorted(forces)]):
            for name,actual in zip(names,row):
                with self.subTest(force=force,curve=name): self.near(actual,hermite(self.curves[name],force))

    def test_velocity_envelope_requests_and_exact_completion(self):
        pairs=[(force,i/100) for force in (200,400,710,750) for i in range(101)]
        for (force,phase),row in self.samples(pairs):
            with self.subTest(force=force,phase=phase):
                if phase==1:
                    self.assertEqual(row[4],0); continue
                normalized=f32(f32(f32(row[1])*phase)/f32(row[1]))
                expected=hermite(self.curves['m_ForceToVelocity'],force)*hermite(self.curves['m_TimeVelocityScale'],normalized)
                self.near(row[4],expected)


if __name__=='__main__': unittest.main()
