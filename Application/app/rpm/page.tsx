"use client";

import { Activity, ChevronLeft, Gauge, History as HistoryIcon } from "lucide-react";
import Link from "next/link";
import { useLiveTelemetry } from "../hooks/useLiveTelemetry";
import Speedometer from "../components/Speedometer";

export default function RpmSensorPage() {
    const { data, connected } = useLiveTelemetry();

    // Calculate RPM bar width (percentage)
    const rpmPercent = Math.min((data.rpm / 8000) * 100, 100);

    return (
        <main className="min-h-screen bg-background text-foreground flex flex-col items-center">
            {/* Premium Glass Header */}
            <header className="w-full h-16 glass-header px-4 flex items-center justify-between sticky top-0 z-50">
                <Link href="/dashboard" className="p-2 hover:bg-white/5 rounded-xl transition border border-transparent hover:border-white/5">
                    <HistoryIcon className="w-5 h-5 text-primary" />
                </Link>
                <h1 className="text-lg font-racing tracking-[0.2em] text-foreground italic">RPM ANALYTICS</h1>
                <div className="flex items-center gap-2">
                    <span className={`w-2 h-2 rounded-full ${connected ? 'bg-highlight shadow-[0_0_10px_rgba(16,185,129,0.5)]' : 'bg-error'}`}></span>
                    <span className="text-[10px] font-racing text-text-secondary uppercase tracking-widest">{connected ? 'Live' : 'Off'}</span>
                </div>
            </header>

            {/* Dashboard Content */}
            <div className="flex-1 w-full max-w-md p-4 flex flex-col items-center justify-center space-y-8">

                {/* RPM Bar - High Performance Display */}
                <div className="w-full glass-card rounded-2xl p-6 relative overflow-hidden border border-white/5 shadow-glow-blue/5">
                    <div className="flex justify-between items-end mb-4 relative z-10">
                        <span className="text-text-secondary font-racing text-[10px] uppercase tracking-widest">Engine Load</span>
                        <div className="text-right">
                            <div className="text-4xl font-data font-bold text-highlight tracking-tighter drop-shadow-glow-green">
                                {Math.round(data.rpm)}
                            </div>
                            <div className="text-[9px] font-racing text-text-secondary tracking-widest -mt-1 uppercase">RPM</div>
                        </div>
                    </div>

                    <div className="w-full bg-background-secondary rounded-xl h-10 overflow-hidden border border-white/5 relative shadow-inner">
                        {/* RPM Fill */}
                        <div
                            className="h-full bg-gradient-to-r from-highlight/40 via-highlight to-primary transition-all duration-75 shadow-[0_0_20px_rgba(16,185,129,0.4)]"
                            style={{ width: `${rpmPercent}%` }}
                        ></div>

                        {/* Redline Marker */}
                        <div className="absolute top-0 right-[15%] bottom-0 w-[2px] bg-red-600/60 shadow-[0_0_15px_rgba(220,38,38,0.8)] z-20"></div>

                        {/* Subtle Grid Lines */}
                        <div className="absolute inset-0 flex justify-between px-2 pointer-events-none opacity-20">
                            {[...Array(10)].map((_, i) => <div key={i} className="w-px h-full bg-white/20"></div>)}
                        </div>
                    </div>

                    <div className="flex justify-between mt-2 text-[10px] font-data text-text-secondary">
                        <span>0</span>
                        <span>2</span>
                        <span>4</span>
                        <span>6</span>
                        <span className="text-primary font-bold">8k</span>
                    </div>
                </div>

                {/* Speed Integration */}
                <div className="flex flex-col items-center justify-center relative p-8 bg-background-secondary/20 rounded-full border border-white/5 aspect-square w-64">
                    <Speedometer />
                    <div className="mt-[-20px] text-center z-10">
                        <div className="text-5xl font-bold font-data text-white tabular-nums tracking-tighter drop-shadow-glow-blue">
                            {Math.round(data.speed)}
                        </div>
                        <div className="text-sm font-racing text-primary tracking-widest mt-1">KM/H</div>
                    </div>
                </div>

                {/* Meta Stats */}
                <div className="grid grid-cols-2 gap-4 w-full">
                    <div className="carbon-bg border border-border-color rounded-xl p-4 flex flex-col items-center">
                        <Activity className="w-5 h-5 text-highlight mb-1" />
                        <span className="text-[10px] text-text-secondary uppercase">GPS Signal</span>
                        <span className="text-lg font-data font-bold text-white">{data.sats} SAT</span>
                    </div>
                    <div className="carbon-bg border border-border-color rounded-xl p-4 flex flex-col items-center">
                        <Gauge className="w-5 h-5 text-primary mb-1" />
                        <span className="text-[10px] text-text-secondary uppercase">Trip Dist.</span>
                        <span className="text-lg font-data font-bold text-white">{data.trip.toFixed(1)} KM</span>
                    </div>
                </div>
            </div>
        </main>
    );
}
