"use client";

import { History, TrendingUp, Trophy, ChevronRight, Activity, Zap, Timer, MapPin } from "lucide-react";
import Link from "next/link";
import BottomNav from "./components/BottomNav";

export default function Home() {
  return (
    <main className="min-h-screen pb-24 bg-background text-foreground">
      {/* Premium Glass Header */}
      <header className="sticky top-0 z-50 glass-header px-4 h-16 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 bg-primary/10 rounded flex items-center justify-center border border-primary/20">
            <History className="w-4 h-4 text-primary" />
          </div>
          <h1 className="text-lg font-racing text-foreground tracking-widest italic">RACING FEED</h1>
        </div>
        <div className="flex items-center gap-2">
          <div className="px-3 py-1 bg-highlight/10 border border-highlight/20 rounded-full flex items-center gap-2">
            <span className="w-1.5 h-1.5 bg-highlight rounded-full animate-pulse"></span>
            <span className="text-[10px] font-racing text-highlight tracking-wider">Cloud Connected</span>
          </div>
        </div>
      </header>

      {/* Hero Stats / Global Feed Summary */}
      <div className="p-4 bg-gradient-to-b from-background-secondary to-background border-b border-white/5">
        <div className="grid grid-cols-3 gap-2">
          <QuickStat label="TOTAL RUNS" value="124" icon={<Zap className="w-3 h-3" />} />
          <QuickStat label="BEST 0-100" value="3.2s" icon={<Trophy className="w-3 h-3 text-warning" />} />
          <QuickStat label="TOP SPEED" value="284" icon={<TrendingUp className="w-3 h-3 text-primary" />} />
        </div>
      </div>

      {/* Feed Content */}
      <div className="px-4 py-6 space-y-8">

        {/* Date Group: Today */}
        <section className="space-y-4">
          <div className="flex items-center gap-3">
            <span className="text-xs font-racing text-text-secondary tracking-[0.2em]">TODAY</span>
            <div className="h-px flex-1 bg-border-color/30"></div>
          </div>

          {/* Session Group Card - Lap Timer Type */}
          <div className="carbon-bg rounded-2xl border border-white/5 overflow-hidden shadow-2xl relative group">
            <div className="absolute top-0 left-0 w-1 h-full bg-highlight group-hover:w-2 transition-all"></div>

            <div className="p-4 bg-background-secondary/50 flex justify-between items-start border-b border-white/5">
              <div>
                <div className="flex items-center gap-2 mb-1">
                  <h3 className="text-sm font-racing text-white">SENTUL KARTING CIRCUIT</h3>
                  <span className="text-[9px] px-1.5 py-0.5 bg-highlight/20 text-highlight border border-highlight/30 rounded uppercase font-bold">Lap Timer</span>
                </div>
                <div className="flex items-center gap-3 text-xs text-text-secondary font-medium">
                  <span className="flex items-center gap-1"><MapPin className="w-3 h-3 text-primary" /> BOGOR, ID</span>
                  <span className="w-1 h-1 bg-border-color rounded-full"></span>
                  <span>15 LAPS</span>
                </div>
              </div>
              <div className="text-right">
                <div className="text-[10px] font-racing text-text-secondary tracking-tighter uppercase mb-1">Fastest</div>
                <div className="text-2xl font-data font-bold text-highlight tracking-tighter shadow-glow-green/10">48.234</div>
              </div>
            </div>

            <Link href="/dashboard" className="flex items-center justify-between p-4 hover:bg-white/5 transition group/item">
              <div className="flex gap-4 items-center">
                <Activity className="w-5 h-5 text-primary" />
                <div>
                  <p className="text-xs font-racing text-white">Full Session Data</p>
                  <p className="text-[10px] text-text-secondary">Analyzed: 14:30 PM • Driver: FARIS</p>
                </div>
              </div>
              <ChevronRight className="w-5 h-5 text-text-secondary group-hover/item:translate-x-1 transition" />
            </Link>
          </div>

          {/* Session Group Card - Drag Meter Type */}
          <div className="carbon-bg rounded-2xl border border-white/5 overflow-hidden shadow-2xl relative group">
            <div className="absolute top-0 left-0 w-1 h-full bg-primary group-hover:w-2 transition-all"></div>

            <div className="p-4 bg-background-secondary/50 flex justify-between items-start border-b border-white/5">
              <div>
                <div className="flex items-center gap-2 mb-1">
                  <h3 className="text-sm font-racing text-white">BSD CITY CIRCUIT</h3>
                  <span className="text-[9px] px-1.5 py-0.5 bg-primary/20 text-primary border border-primary/30 rounded uppercase font-bold">Drag Meter</span>
                </div>
                <div className="flex items-center gap-3 text-xs text-text-secondary font-medium">
                  <span className="flex items-center gap-1"><Zap className="w-3 h-3 text-primary" /> 100-200 KPH</span>
                  <span className="w-1 h-1 bg-border-color rounded-full"></span>
                  <span>4 RUNS</span>
                </div>
              </div>
              <div className="text-right">
                <div className="text-[10px] font-racing text-text-secondary tracking-tighter uppercase mb-1">Best Time</div>
                <div className="text-2xl font-data font-bold text-primary tracking-tighter shadow-glow-blue/10">5.451</div>
              </div>
            </div>

            <div className="divide-y divide-white/5">
              <RunItem index="04" value="5.832" delta="-0.21s" valid />
              <RunItem index="03" value="5.451" delta="---" valid best />
              <RunItem index="02" value="6.120" delta="+0.45s" valid={false} />
            </div>
          </div>

        </section>

        {/* Previous Days Feed */}
        <section className="space-y-4 opacity-70 grayscale-[0.5]">
          <div className="flex items-center gap-3">
            <span className="text-xs font-racing text-text-secondary tracking-[0.2em]">YESTERDAY</span>
            <div className="h-px flex-1 bg-border-color/30"></div>
          </div>
          {/* Session Card for Yesterday */}
          <div className="carbon-bg rounded-xl border border-white/5 p-4 flex justify-between items-center">
            <div className="flex gap-4 items-center">
              <div className="w-10 h-10 rounded bg-background-secondary border border-white/5 flex items-center justify-center">
                <Timer className="w-5 h-5 text-text-secondary" />
              </div>
              <div>
                <p className="text-xs font-racing text-white uppercase">Sentul practice</p>
                <p className="text-[9px] text-text-secondary uppercase tracking-widest mt-1">12 LAPS • BEST: 49.1</p>
              </div>
            </div>
            <ChevronRight className="w-4 h-4 text-text-secondary" />
          </div>
        </section>
      </div>

      <BottomNav />
    </main>
  );
}

function QuickStat({ label, value, icon }: { label: string; value: string; icon: React.ReactNode }) {
  return (
    <div className="carbon-bg rounded-xl border border-white/5 p-2 flex flex-col items-center">
      <div className="flex items-center gap-1 mb-1">
        {icon}
        <span className="text-[8px] font-racing text-text-secondary tracking-wider">{label}</span>
      </div>
      <div className="text-sm font-data font-bold text-white tracking-tighter">{value}</div>
    </div>
  );
}

function RunItem({ index, value, delta, valid, best = false }: { index: string; value: string; delta: string; valid: boolean; best?: boolean }) {
  return (
    <Link href="/session/1" className="flex items-center justify-between p-3 px-4 hover:bg-white/5 transition group/item">
      <div className="flex gap-4 items-center">
        <span className={`text-[10px] font-racing ${best ? 'text-highlight' : 'text-text-secondary'}`}>{index}</span>
        <div className="flex flex-col">
          <span className={`text-xl font-data font-bold ${best ? 'text-highlight' : 'text-white'}`}>{value}</span>
          <span className="text-[8px] text-text-secondary uppercase">100-200 KM/H</span>
        </div>
      </div>
      <div className="text-right">
        <div className={`text-[10px] font-data ${delta.startsWith('-') ? 'text-highlight' : 'text-error'}`}>{delta}</div>
        <div className={`text-[8px] font-racing uppercase tracking-widest ${valid ? 'text-highlight' : 'text-error'}`}>
          {valid ? '✓ Valid' : '✗ Invalid'}
        </div>
      </div>
    </Link>
  );
}
