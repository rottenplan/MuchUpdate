"use client";

import { ChevronLeft, MapPin, Radio, Satellite, ShieldCheck, Zap, History as HistoryIcon } from "lucide-react";
import Link from "next/link";
import { useLiveTelemetry } from "../hooks/useLiveTelemetry";

export default function GpsStatusPage() {
    const { data, connected } = useLiveTelemetry();

    // Mock Signal Strength Calculation (for visualization)
    const signalStrength = data.sats > 10 ? 4 : data.sats > 7 ? 3 : data.sats > 4 ? 2 : data.sats > 0 ? 1 : 0;

    return (
        <main className="min-h-screen bg-background text-foreground flex flex-col items-center">
            {/* Premium Glass Header */}
            <header className="w-full h-16 glass-header px-4 flex items-center justify-between sticky top-0 z-50">
                <Link href="/dashboard" className="p-2 hover:bg-white/5 rounded-xl transition border border-transparent hover:border-white/5">
                    <HistoryIcon className="w-5 h-5 text-primary" />
                </Link>
                <h1 className="text-lg font-racing tracking-[0.2em] text-foreground italic">GPS STATUS</h1>
                <div className={`w-2 h-2 rounded-full ${connected ? 'bg-highlight shadow-[0_0_10px_rgba(16,185,129,0.5)]' : 'bg-error'}`}></div>
            </header>

            {/* Main Signal Display */}
            <div className="flex-1 w-full max-w-md p-4 flex flex-col space-y-6">

                {/* Signal Strength Huge Card */}
                <div className="glass-card rounded-2xl p-8 flex flex-col items-center justify-center relative overflow-hidden group shadow-glow-blue/5 border border-white/5">
                    <div className="absolute -top-10 -right-10 w-40 h-40 bg-highlight/5 rounded-full blur-3xl group-hover:bg-highlight/10 transition-all duration-700"></div>

                    <Satellite className={`w-16 h-16 ${data.sats > 0 ? 'text-highlight' : 'text-text-secondary'} mb-4 drop-shadow-[0_0_15px_rgba(16,185,129,0.2)]`} />

                    <div className="text-6xl font-data font-bold text-white mb-2 drop-shadow-glow-blue">{data.sats}</div>
                    <div className="text-[10px] font-racing text-text-secondary uppercase tracking-[0.3em] font-medium opacity-60">Satellites Fixed</div>

                    {/* Signal Bars */}
                    <div className="flex items-end gap-2 mt-8 h-12">
                        {[1, 2, 3, 4].map((bar) => (
                            <div
                                key={bar}
                                className={`w-4 rounded-full transition-all duration-1000 ${bar <= signalStrength ? 'bg-gradient-to-t from-highlight/50 to-highlight shadow-[0_0_15px_rgba(16,185,129,0.4)]' : 'bg-white/5'}`}
                                style={{ height: `${bar * 25}%` }}
                            ></div>
                        ))}
                    </div>
                </div>

                {/* Status Grid */}
                <div className="grid grid-cols-2 gap-4">
                    <StatusTile
                        icon={<Radio className="w-5 h-5" />}
                        label="FIX TYPE"
                        value={data.sats > 3 ? "3D FIX" : data.sats > 0 ? "2D FIX" : "NO FIX"}
                        color={data.sats > 3 ? "highlight" : "warning"}
                    />
                    <StatusTile
                        icon={<ShieldCheck className="w-5 h-5" />}
                        label="ACCURACY"
                        value={data.sats > 10 ? "HIGH" : data.sats > 6 ? "MED" : "LOW"}
                        color={data.sats > 10 ? "highlight" : data.sats > 6 ? "warning" : "error"}
                    />
                    <StatusTile
                        icon={<MapPin className="w-5 h-5" />}
                        label="LATITUDE"
                        value={data.lat.toFixed(5)}
                        mono
                    />
                    <StatusTile
                        icon={<MapPin className="w-5 h-5" />}
                        label="LONGITUDE"
                        value={data.lng.toFixed(5)}
                        mono
                    />
                </div>

                {/* Live Logs Mockup (Mirroring Firmware Debug) */}
                <div className="glass-card rounded-2xl p-5 flex-1 font-data text-[10px] text-highlight/70 overflow-hidden border border-white/5 shadow-inner">
                    <div className="flex justify-between border-b border-white/5 pb-3 mb-3">
                        <span className="font-racing text-[10px] text-text-secondary tracking-widest uppercase opacity-60">NMEA RAW STREAM</span>
                        <div className="flex items-center gap-2">
                            <div className="w-1.5 h-1.5 bg-highlight rounded-full animate-pulse"></div>
                            <span className="text-[9px] font-racing text-highlight tracking-widest">RECORDING</span>
                        </div>
                    </div>
                    <div className="space-y-1.5 opacity-40 font-data">
                        <p>$GNGGA,061853.00,0644.444,S,10644.444,E,1,12,0.8,...</p>
                        <p>$GNRMC,061853.00,A,0644.444,S,10644.444,E,0.00,0.00,...</p>
                        <p>$GNVTG,0.00,T,,M,0.00,N,0.00,K,A*3B</p>
                        <p>$GNGSA,A,3,01,03,05,07,08,11,17,19,22,28,,,1.5,0.8,1.3*06</p>
                    </div>
                </div>

            </div>
        </main>
    );
}

function StatusTile({ icon, label, value, color = "primary", mono = false }: { icon: React.ReactNode; label: string; value: string; color?: string; mono?: boolean }) {
    const colorMap: Record<string, string> = {
        primary: "text-primary",
        highlight: "text-highlight",
        warning: "text-warning",
        error: "text-error"
    };

    return (
        <div className="glass-card rounded-2xl p-4 flex flex-col items-center group hover:border-primary/50 transition-all duration-500 border border-white/5">
            <div className={`${colorMap[color]} mb-3 group-hover:scale-110 transition-transform duration-500`}>{icon}</div>
            <span className="text-[9px] text-text-secondary uppercase mb-1 tracking-widest opacity-60 font-racing">{label}</span>
            <span className={`text-sm font-bold ${mono ? 'font-data' : 'font-racing'} text-white tracking-tighter`}>{value}</span>
        </div>
    );
}
