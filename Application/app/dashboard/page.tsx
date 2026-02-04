'use client';

import Link from 'next/link';
import { Clock, TrendingUp, MapPin, Calendar, Search, Edit, Trash2, Upload, ChevronLeft, ChevronRight, Zap, Trophy, Activity, Satellite, Gauge, History, Settings, RefreshCw } from 'lucide-react';
import { useState } from 'react';
import MapWrapper from "../components/MapWrapper";
import Speedometer from "../components/Speedometer";
import DragModeView from "../components/DragModeView";

// Mock data - expanded
const stats = {
  totalSessions: 24,
  totalLaps: 486,
  bestLapTime: '48.234',
  avgLapTime: '52.891',
};

const allSessions = [
  { id: 1, driver: 'FARIS', track: 'Sentul Karting Circuit', city: 'Bogor', date: '2026-01-06', time: '14:30', category: 'Vespa Tune Up', laps: 25, bestLap: '48.234' },
  { id: 2, driver: 'FARIS', track: 'BSD Karting Track', city: 'Tangerang', date: '2026-01-05', time: '16:45', category: 'Vespa Tune Up', laps: 18, bestLap: '45.891' },
  { id: 3, driver: 'FARIS', track: 'Sentul Karting Circuit', city: 'Bogor', date: '2026-01-03', time: '10:20', category: 'Vespa Tune Up', laps: 22, bestLap: '52.891' },
  { id: 4, driver: 'FARIS', track: 'Genk Karting Belgium', city: 'Genk', date: '2025-12-28', time: '15:00', category: 'Rental Kart', laps: 20, bestLap: '49.123' },
  { id: 5, driver: 'FARIS', track: 'BSD Karting Track', city: 'Tangerang', date: '2025-12-20', time: '11:30', category: 'Vespa Tune Up', laps: 16, bestLap: '46.234' },
];

const trackGroups = [
  { id: 1, name: 'Sentul Karting Circuit', city: 'Bogor', country: 'Indonesia', sessions: 8, flag: '🇮🇩' },
  { id: 2, name: 'BSD Karting Track', city: 'Tangerang', country: 'Indonesia', sessions: 6, flag: '🇮🇩' },
  { id: 3, name: 'Genk Karting Belgium', city: 'Genk', country: 'Belgium', sessions: 4, flag: '🇧🇪' },
];

export default function DashboardPage() {
  const [searchQuery, setSearchQuery] = useState('');
  const [rowsPerPage, setRowsPerPage] = useState(10);
  const [currentPage, setCurrentPage] = useState(1);
  const [viewMode, setViewMode] = useState<'track' | 'drag'>('track');

  const filteredSessions = allSessions.filter(session =>
    session.track.toLowerCase().includes(searchQuery.toLowerCase()) ||
    session.city.toLowerCase().includes(searchQuery.toLowerCase()) ||
    session.category.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const totalPages = Math.ceil(filteredSessions.length / rowsPerPage);
  const startIndex = (currentPage - 1) * rowsPerPage;
  const displayedSessions = filteredSessions.slice(startIndex, startIndex + rowsPerPage);

  return (
    <div className="min-h-screen bg-background text-foreground pb-24">
      {/* Header with Carbon Fiber */}


      {/* Dashboard Content */}
      <div className="container mx-auto px-4 py-6 space-y-6">

        {/* Stats Grid - High Contrast Analytics */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <StatCard
            icon={<Trophy className="w-5 h-5" />}
            label="BEST LAP"
            value={stats.bestLapTime}
            subtext="Sentul - 2026-01-06"
            color="highlight"
          />
          <StatCard
            icon={<TrendingUp className="w-5 h-5" />}
            label="TOTAL LAPS"
            value={stats.totalLaps.toString()}
            subtext="All Sessions"
            color="primary"
          />
          <StatCard
            icon={<MapPin className="w-5 h-5" />}
            label="LIFETIME"
            value="1,245 km"
            subtext="Driven Distance"
            color="warning"
          />
          <StatCard
            icon={<Clock className="w-5 h-5" />}
            label="ENGINE"
            value="24.5 h"
            subtext="Active Time"
            color="primary"
          />
        </div>

        {/* Live Telemetry Section */}
        <div>
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-xl font-racing flex items-center gap-2">
              <MapPin className="w-5 h-5 text-primary" />
              LIVE TELEMETRY
            </h2>
            {/* View Toggle */}
            <div className="flex bg-background-secondary rounded-lg p-1 border border-border-color">
              <button
                onClick={() => setViewMode('track')}
                className={`px-3 py-1 rounded text-xs font-racing transition-all ${viewMode === 'track' ? 'bg-primary text-white shadow-lg' : 'text-text-secondary hover:text-white'}`}
              >
                TRACK
              </button>
              <button
                onClick={() => setViewMode('drag')}
                className={`px-3 py-1 rounded text-xs font-racing transition-all ${viewMode === 'drag' ? 'bg-primary text-white shadow-lg' : 'text-text-secondary hover:text-white'}`}
              >
                DRAG
              </button>
            </div>
          </div>

          <div className="h-[400px]">
            {viewMode === 'track' ? (
              <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 h-full">
                {/* Map - Takes up 2 columns on large screens */}
                <div className="lg:col-span-2 carbon-bg rounded-xl overflow-hidden border border-border-color shadow-lg relative">
                  <MapWrapper />
                </div>
                {/* Speedometer - Compact side panel */}
                <div className="carbon-bg rounded-xl border border-border-color shadow-lg flex items-center justify-center relative bg-[radial-gradient(ellipse_at_center,_var(--tw-gradient-stops))] from-background-secondary to-background">
                  <Speedometer />
                  {/* Live Indicator */}
                  <div className="absolute top-4 right-4 flex items-center gap-2">
                    <span className="relative flex h-3 w-3">
                      <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-error opacity-75"></span>
                      <span className="relative inline-flex rounded-full h-3 w-3 bg-error"></span>
                    </span>
                    <span className="text-xs font-bold text-error">LIVE</span>
                  </div>
                </div>
              </div>
            ) : (
              /* Drag Mode View - Full Width */
              <div className="w-full h-full carbon-bg rounded-xl border border-border-color shadow-lg relative overflow-hidden bg-[radial-gradient(ellipse_at_top,_var(--tw-gradient-stops))] from-slate-900 to-background">
                <DragModeView />
              </div>
            )}
          </div>
        </div>

        {/* 8-Module HUB Grid */}
        <div className="space-y-4">
          <div className="flex items-center gap-3">
            <h2 className="text-xl font-racing text-primary tracking-[0.2em] italic">PRO HUB</h2>
            <div className="h-px flex-1 bg-gradient-to-r from-primary/30 to-transparent"></div>
          </div>
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
            <ModuleTile icon={<Trophy className="w-6 h-6" />} label="LAP TIMER" href="/tracks" color="highlight" />
            <ModuleTile icon={<Zap className="w-6 h-6" />} label="DRAG METER" href="#" onClick={() => { setViewMode('drag'); window.scrollTo({ top: 0, behavior: 'smooth' }); }} color="primary" />
            <ModuleTile icon={<Activity className="w-6 h-6" />} label="RPM SENSOR" href="/rpm" color="highlight" />
            <ModuleTile icon={<Gauge className="w-6 h-6" />} label="SPEEDO" href="#" onClick={() => { setViewMode('track'); window.scrollTo({ top: 0, behavior: 'smooth' }); }} color="primary" />
            <ModuleTile icon={<History className="w-6 h-6" />} label="HISTORY" href="/" color="warning" />
            <ModuleTile icon={<Satellite className="w-6 h-6" />} label="GPS STATUS" href="/gps" color="highlight" />
            <ModuleTile icon={<Settings className="w-6 h-6" />} label="SETTINGS" href="/device" color="text-secondary" />
            <ModuleTile icon={<RefreshCw className="w-6 h-6" />} label="SYNC" href="#" color="primary" />
          </div>
        </div>

        {/* GPX Upload Widget */}
        <div className="carbon-bg border border-border-color rounded-xl p-4">
          <h2 className="text-xl font-racing mb-4 flex items-center gap-2">
            <Upload className="w-5 h-5 text-primary" />
            UPLOAD GPX
          </h2>

          <div className="border-2 border-dashed border-border-color rounded-xl p-6 text-center hover:border-primary/50 transition cursor-pointer group relative">
            <input
              type="file"
              multiple
              accept=".gpx"
              className="absolute inset-0 w-full h-full opacity-0 cursor-pointer"
              onChange={async (e) => {
                const files = e.target.files;
                if (files && files.length > 0) {
                  const progressBar = document.getElementById('progress_bar');
                  const progressText = document.getElementById('progress_text');

                  if (progressBar && progressText) {
                    progressBar.style.width = '10%';
                    progressText.innerText = 'Uploading...';
                  }

                  for (let i = 0; i < files.length; i++) {
                    const file = files[i];
                    const formData = new FormData();
                    formData.append('file', file);

                    try {
                      const res = await fetch('/api/upload/gpx', { method: 'POST', body: formData });
                      const data = await res.json();

                      if (res.ok) {
                        if (progressBar) progressBar.style.width = '100%';
                        if (progressText) progressText.innerText = 'Done!';
                        setTimeout(() => alert(`Uploaded: ${file.name}`), 100);
                        window.location.reload();
                      } else {
                        alert(`Failed to upload ${file.name}: ${data.error}`);
                      }
                    } catch (err) {
                      console.error(err);
                      alert(`Error uploading ${file.name}`);
                    }
                  }
                }
              }}
            />
            <div className="flex flex-col items-center justify-center pointer-events-none">
              <div className="bg-background-secondary p-3 rounded-full mb-3 group-hover:bg-card-bg transition">
                <Upload className="w-6 h-6 text-text-secondary group-hover:text-primary transition" />
              </div>
              <p className="text-base text-foreground font-racing mb-1">
                DRAG & DROP GPX
              </p>
              <p className="text-text-secondary text-xs">
                or click to select
              </p>
            </div>
          </div>

          {/* Progress Bar */}
          <div className="mt-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-text-secondary text-xs font-medium">Upload Status</span>
              <span id="progress_text" className="text-primary text-xs font-data font-bold"></span>
            </div>
            <div className="w-full bg-background-secondary rounded-full h-2 overflow-hidden">
              <div id="progress_bar" className="bg-gradient-to-r from-primary to-highlight h-full rounded-full transition-all duration-200" style={{ width: '0%' }}></div>
            </div>
          </div>
        </div>

        {/* All My Sessions Table */}
        <div className="carbon-bg border border-border-color rounded-xl p-4">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-xl font-racing">ALL SESSIONS</h2>
            <button className="bg-primary hover:bg-primary-hover text-white px-3 py-2 rounded-lg transition flex items-center gap-2 text-sm font-racing">
              <Upload className="w-4 h-4" />
              UPLOAD
            </button>
          </div>

          {/* Search */}
          <div className="mb-4">
            <div className="relative">
              <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-text-secondary w-4 h-4" />
              <input
                type="text"
                placeholder="Search sessions..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-background-secondary border border-border-color rounded-lg pl-10 pr-4 py-2 text-foreground placeholder-text-secondary focus:outline-none focus:border-primary transition"
              />
            </div>
          </div>

          {/* Table */}
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-border-color">
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">ID</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">DRIVER</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">DATE</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">TRACK</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">LAPS</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2">BEST</th>
                  <th className="text-left text-text-secondary font-racing text-xs py-2 px-2"></th>
                </tr>
              </thead>
              <tbody>
                {displayedSessions.map((session) => (
                  <tr key={session.id} className="border-b border-border-color/50 hover:bg-card-bg transition">
                    <td className="py-2 px-2 text-text-secondary font-data">{session.id}</td>
                    <td className="py-2 px-2 text-foreground font-medium">{session.driver}</td>
                    <td className="py-2 px-2 text-text-secondary text-xs">
                      <Link href={`/sessions/${session.id}`} className="text-primary hover:text-primary-hover transition">
                        {session.date}
                      </Link>
                    </td>
                    <td className="py-2 px-2 text-foreground text-xs">{session.track}</td>
                    <td className="py-2 px-2 text-foreground font-data">{session.laps}</td>
                    <td className="py-2 px-2 text-highlight font-data font-bold">{session.bestLap}</td>
                    <td className="py-2 px-2">
                      <div className="flex gap-2">
                        <button className="text-primary hover:text-primary-hover transition">
                          <Edit className="w-4 h-4" />
                        </button>
                        <button className="text-warning hover:text-foreground transition">
                          <Trash2 className="w-4 h-4" />
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          {/* Pagination */}
          <div className="flex items-center justify-between mt-4 text-xs">
            <div className="flex items-center gap-2">
              <span className="text-text-secondary">Show</span>
              <select
                value={rowsPerPage}
                onChange={(e) => setRowsPerPage(Number(e.target.value))}
                className="bg-background-secondary border border-border-color rounded px-2 py-1 text-foreground focus:outline-none focus:border-primary"
              >
                <option value={10}>10</option>
                <option value={25}>25</option>
                <option value={50}>50</option>
              </select>
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={() => setCurrentPage(Math.max(1, currentPage - 1))}
                disabled={currentPage === 1}
                className="p-2 bg-background-secondary border border-border-color rounded hover:border-primary transition disabled:opacity-50"
              >
                <ChevronLeft className="w-4 h-4 text-foreground" />
              </button>
              <span className="text-text-secondary font-data">
                {currentPage} / {totalPages}
              </span>
              <button
                onClick={() => setCurrentPage(Math.min(totalPages, currentPage + 1))}
                disabled={currentPage === totalPages}
                className="p-2 bg-background-secondary border border-border-color rounded hover:border-primary transition disabled:opacity-50"
              >
                <ChevronRight className="w-4 h-4 text-foreground" />
              </button>
            </div>
          </div>
        </div>

        {/* Track Groups */}
        <div>
          <h2 className="text-xl font-racing mb-4">SESSIONS BY TRACK</h2>
          <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-4">
            {trackGroups.map((track) => (
              <TrackGroupCard key={track.id} track={track} />
            ))}
          </div>
        </div>
      </div>

    </div>
  );
}

function StatCard({ icon, label, value, subtext, color }: { icon: React.ReactNode; label: string; value: string; subtext: string; color: string }) {
  const colorGlows = {
    primary: 'shadow-glow-blue',
    highlight: 'shadow-glow-green',
    warning: 'shadow-glow-blue', // fallback
  };

  const colorTexts = {
    primary: 'text-primary',
    highlight: 'text-highlight',
    warning: 'text-warning',
  };

  return (
    <div className={`glass-card rounded-2xl p-4 relative overflow-hidden group hover:border-primary/50 transition duration-500 ${colorGlows[color as keyof typeof colorGlows] ? 'hover:' + colorGlows[color as keyof typeof colorGlows] : ''}`}>
      <div className={`absolute -top-2 -right-2 p-4 opacity-5 group-hover:opacity-10 transition-opacity ${colorTexts[color as keyof typeof colorTexts]}`}>
        {icon}
      </div>
      <div className="text-text-secondary text-[10px] font-racing mb-2 tracking-[0.1em]">{label}</div>
      <div className={`text-2xl font-data font-bold tracking-tighter mb-1 ${colorTexts[color as keyof typeof colorTexts]}`}>{value}</div>
      <div className="text-text-secondary text-[10px] opacity-60 font-medium uppercase">{subtext}</div>
    </div>
  );
}

function TrackGroupCard({ track }: { track: any }) {
  return (
    <Link href={`/tracks/${track.id}`}>
      <div className="carbon-bg border border-border-color rounded-xl overflow-hidden hover:border-primary/50 transition group">
        {/* Track Map Placeholder */}
        <div className="h-32 bg-gradient-to-br from-background-secondary to-background flex items-center justify-center">
          <MapPin className="w-10 h-10 text-text-secondary group-hover:text-primary transition" />
        </div>

        {/* Track Info */}
        <div className="p-4">
          <div className="flex items-start justify-between mb-2">
            <h3 className="text-foreground font-racing text-sm group-hover:text-primary transition">
              {track.name}
            </h3>
            <span className="text-xl">{track.flag}</span>
          </div>

          <p className="text-text-secondary text-xs mb-3">
            {track.city}, {track.country}
          </p>

          <div className="flex items-center justify-between">
            <span className="text-text-secondary text-xs font-racing">SESSIONS</span>
            <span className="text-primary font-data font-bold text-lg">{track.sessions}</span>
          </div>
        </div>
      </div>
    </Link>
  );
}

function ModuleTile({ icon, label, href, onClick, color = 'primary' }: { icon: React.ReactNode; label: string; href: string; onClick?: () => void; color?: string }) {
  const colorTexts = {
    primary: 'text-primary',
    highlight: 'text-highlight',
    warning: 'text-warning',
    'text-secondary': 'text-text-secondary',
  };

  const content = (
    <div
      onClick={onClick}
      className="glass-card rounded-2xl p-4 flex flex-col items-center justify-center gap-3 hover:bg-white/5 transition-all duration-300 group cursor-pointer aspect-square sm:aspect-auto sm:h-32 border border-white/5"
    >
      <div className={`p-4 rounded-2xl bg-background-secondary group-hover:bg-white/5 transition-all duration-500 ${colorTexts[color as keyof typeof colorTexts]} group-hover:scale-110 shadow-inner`}>
        {icon}
      </div>
      <span className="text-[10px] font-racing text-white group-hover:text-primary transition-colors tracking-[0.2em]">{label}</span>
    </div>
  );

  if (href === "#") return content;
  return <Link href={href} className="block">{content}</Link>;
}
