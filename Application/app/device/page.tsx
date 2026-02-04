'use client';

import Link from 'next/link';
import { Wifi, Upload, Download, Settings, HardDrive, Cpu, Globe, Gauge, Save, Zap, FileText, Activity, Scaling } from 'lucide-react';
import { useState, useEffect } from 'react';


const countries = [
  'Argentina', 'Australia', 'Austria', 'Belgium', 'Brazil', 'Canada', 'Chile', 'China',
  'Colombia', 'Czech Republic', 'Denmark', 'Finland', 'France', 'Germany', 'Greece',
  'Hong Kong', 'Hungary', 'India', 'Indonesia', 'Ireland', 'Italy', 'Japan', 'Malaysia',
  'Mexico', 'Netherlands', 'New Zealand', 'Norway', 'Philippines', 'Poland', 'Portugal',
  'Russia', 'Singapore', 'South Africa', 'South Korea', 'Spain', 'Sweden', 'Switzerland',
  'Taiwan', 'Thailand', 'Turkey', 'UAE', 'United Kingdom', 'USA', 'Vietnam'
];

export default function DevicePage() {
  const [selectedCountries, setSelectedCountries] = useState<string[]>(['Indonesia', 'Malaysia', 'Singapore']);
  const [activeEngine, setActiveEngine] = useState('1');
  const [units, setUnits] = useState('kmh');
  const [temperature, setTemperature] = useState('celsius');
  const [gnss, setGnss] = useState('gps');
  const [contrast, setContrast] = useState(50);
  const [powerSave, setPowerSave] = useState(5);

  // Storage Stats
  const [storageStats, setStorageStats] = useState({ used: 0, total: 0 });

  useEffect(() => {
    // Fetch real device status
    fetch('/api/device/status')
      .then(res => res.json())
      .then(data => {
        if (data && typeof data.storage_used === 'number') {
          setStorageStats({
            used: data.storage_used,
            total: data.storage_total
          });
        }
      })
      .catch(err => console.error('Failed to fetch stats:', err));
  }, []);

  const toggleCountry = (country: string) => {
    setSelectedCountries(prev =>
      prev.includes(country)
        ? prev.filter(c => c !== country)
        : [...prev, country]
    );
  };

  return (
    <div className="min-h-screen bg-background text-foreground pb-24">


      {/* Content */}
      <div className="container mx-auto px-4 py-6 space-y-6">

        {/* Device Status */}
        <div className="carbon-bg border border-border-color rounded-xl p-6">
          <div className="flex items-center justify-between mb-4">
            <div>
              <h2 className="text-xl font-racing text-foreground mb-1">MuchRacing_e05a1b</h2>
              <p className="text-text-secondary text-xs font-data">MAC: E0:5A:1B:A2:74:44</p>
              <p className="text-text-secondary text-xs font-data">Firmware: v2.1.0</p>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-2 h-2 bg-highlight rounded-full animate-pulse"></div>
              <span className="text-highlight font-racing text-sm">CONNECTED</span>
            </div>
          </div>

          {/* Storage */}
          <div className="mb-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-text-secondary flex items-center gap-2 text-sm">
                <HardDrive className="w-4 h-4" />
                Storage Usage
              </span>
              <span className="text-foreground font-data text-sm font-bold">
                {storageStats.used} MB / {storageStats.total > 0 ? storageStats.total : '---'} MB
              </span>
            </div>
            <div className="w-full bg-background-secondary rounded-full h-2">
              <div
                className="bg-gradient-to-r from-primary to-highlight h-2 rounded-full transition-all"
                style={{ width: `${storageStats.total > 0 ? (storageStats.used / storageStats.total) * 100 : 0}%` }}
              ></div>
            </div>
          </div>

          <div className="grid md:grid-cols-3 gap-4">
            <div className="bg-background-secondary border border-border-color rounded-lg p-3">
              <Wifi className="w-6 h-6 text-primary mb-2" />
              <div className="text-text-secondary text-xs mb-1">WiFi Status</div>
              <div className="text-foreground font-racing text-sm">CONNECTED</div>
            </div>
            <div className="bg-background-secondary border border-border-color rounded-lg p-3">
              <Upload className="w-6 h-6 text-highlight mb-2" />
              <div className="text-text-secondary text-xs mb-1">Last Sync</div>
              <div className="text-foreground font-data text-sm">2 hours ago</div>
            </div>
            <div className="bg-background-secondary border border-border-color rounded-lg p-3">
              <Download className="w-6 h-6 text-highlight mb-2" />
              <div className="text-text-secondary text-xs mb-1">Sessions</div>
              <div className="text-foreground font-data text-sm">24 synced</div>
            </div>
          </div>
        </div>

        {/* Device Settings */}
        <div className="carbon-bg border border-border-color rounded-xl p-6">
          <h2 className="text-lg font-racing text-foreground mb-4 flex items-center gap-2">
            <Settings className="w-5 h-5 text-primary" />
            DEVICE SETTINGS
          </h2>

          <div className="grid md:grid-cols-2 gap-4">
            {/* Units */}
            <div>
              <label className="text-text-secondary text-xs mb-2 block">Speed Units</label>
              <select
                value={units}
                onChange={(e) => setUnits(e.target.value)}
                className="w-full bg-background-secondary border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
              >
                <option value="kmh">Km/h</option>
                <option value="mph">Mph</option>
              </select>
            </div>

            {/* Temperature */}
            <div>
              <label className="text-text-secondary text-xs mb-2 block">Temperature Units</label>
              <select
                value={temperature}
                onChange={(e) => setTemperature(e.target.value)}
                className="w-full bg-background-secondary border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
              >
                <option value="celsius">Celsius</option>
                <option value="fahrenheit">Fahrenheit</option>
              </select>
            </div>

            {/* GNSS */}
            <div>
              <label className="text-text-secondary text-xs mb-2 block">GNSS Mode</label>
              <select
                value={gnss}
                onChange={(e) => setGnss(e.target.value)}
                className="w-full bg-background-secondary border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
              >
                <option value="gps">GPS</option>
                <option value="gps_glonass">GPS + GLONASS</option>
                <option value="gps_galileo">GPS + Galileo</option>
                <option value="all">All (GPS + GLONASS + Galileo)</option>
              </select>
            </div>

            {/* Contrast */}
            <div>
              <label className="text-text-secondary text-xs mb-2 block">Screen Contrast: {contrast}%</label>
              <input
                type="range"
                min="0"
                max="100"
                value={contrast}
                onChange={(e) => setContrast(Number(e.target.value))}
                className="w-full accent-primary"
              />
            </div>

            {/* Power Save */}
            <div className="md:col-span-2">
              <label className="text-text-secondary text-xs mb-2 block">Power Save (minutes)</label>
              <select
                value={powerSave}
                onChange={(e) => setPowerSave(Number(e.target.value))}
                className="w-full bg-background-secondary border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
              >
                <option value={1}>1 minute</option>
                <option value={5}>5 minutes</option>
                <option value={10}>10 minutes</option>
                <option value={30}>30 minutes</option>
                <option value={0}>Never</option>
              </select>
            </div>
          </div>

          <button className="mt-4 bg-primary hover:bg-primary-hover text-white px-4 py-2 rounded-lg transition flex items-center gap-2 font-racing text-sm">
            <Save className="w-4 h-4" />
            SAVE SETTINGS
          </button>
        </div>

        {/* UTILITY - Mirroring Firmware */}
        <div className="carbon-bg border border-border-color rounded-xl p-6">
          <h2 className="text-lg font-racing text-foreground mb-4 flex items-center gap-2">
            <Activity className="w-5 h-5 text-highlight" />
            UTILITY & MAINTENANCE
          </h2>

          <div className="grid md:grid-cols-2 gap-6">
            {/* SD Card Health */}
            <div className="bg-background-secondary/50 border border-border-color rounded-xl p-4 flex flex-col justify-between">
              <div>
                <div className="flex items-center gap-2 mb-2">
                  <HardDrive className="w-4 h-4 text-primary" />
                  <span className="text-sm font-racing">SD CARD HEALTH</span>
                </div>
                <p className="text-text-secondary text-xs mb-4">Run a read/write benchmark to verify SD card integrity.</p>
              </div>
              <div className="flex items-center justify-between">
                <span className="text-[10px] font-data text-highlight">STATUS: READY</span>
                <button className="bg-white/5 hover:bg-white/10 border border-white/10 text-white px-3 py-1.5 rounded-lg text-[10px] font-racing transition">RUN TEST</button>
              </div>
            </div>

            {/* G-Force Calibration */}
            <div className="bg-background-secondary/50 border border-border-color rounded-xl p-4 flex flex-col justify-between">
              <div>
                <div className="flex items-center gap-2 mb-2">
                  <Scaling className="w-4 h-4 text-primary" />
                  <span className="text-sm font-racing">G-FORCE CALIB</span>
                </div>
                <p className="text-text-secondary text-xs mb-4">Calibrate the accelerometer on a perfectly level surface.</p>
              </div>
              <div className="flex items-center justify-between">
                <span className="text-[10px] font-data text-text-secondary">LAST: 2024-11-20</span>
                <button className="bg-white/5 hover:bg-white/10 border border-white/10 text-white px-3 py-1.5 rounded-lg text-[10px] font-racing transition">CALIBRATE</button>
              </div>
            </div>

            {/* GPS Debug Log */}
            <div className="md:col-span-2 bg-background-secondary/50 border border-border-color rounded-xl p-4 flex items-center justify-between">
              <div className="flex items-center gap-4">
                <div className="p-3 bg-primary/10 rounded-lg">
                  <FileText className="w-5 h-5 text-primary" />
                </div>
                <div>
                  <div className="text-sm font-racing">GPS DEBUG LOG</div>
                  <p className="text-text-secondary text-[10px]">Record raw NMEA data to SD card for troubleshooting.</p>
                </div>
              </div>
              <label className="relative inline-flex items-center cursor-pointer">
                <input type="checkbox" className="sr-only peer" />
                <div className="w-11 h-6 bg-gray-700 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
              </label>
            </div>
          </div>
        </div>

        {/* Engine Management */}
        <div className="carbon-bg border border-border-color rounded-xl p-6">
          <h2 className="text-lg font-racing text-foreground mb-4 flex items-center gap-2">
            <Cpu className="w-5 h-5 text-primary" />
            ENGINE MANAGEMENT
          </h2>

          <div className="mb-4">
            <label className="text-text-secondary text-xs mb-2 block">Active Engine</label>
            <select
              value={activeEngine}
              onChange={(e) => setActiveEngine(e.target.value)}
              className="w-full bg-background-secondary border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
            >
              <option value="1">Engine 1</option>
              <option value="2">Engine 2</option>
              <option value="3">Engine 3</option>
            </select>
          </div>

          <div className="space-y-3">
            {[1, 2, 3].map((engineNum) => (
              <div key={engineNum} className="bg-background-secondary border border-border-color rounded-lg p-3">
                <div className="grid md:grid-cols-2 gap-3">
                  <div>
                    <label className="text-text-secondary text-xs mb-1 block">Engine {engineNum} Name</label>
                    <input
                      type="text"
                      defaultValue={engineNum === 1 ? 'Much Racing' : `Engine ${engineNum}`}
                      className="w-full bg-background border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm"
                    />
                  </div>
                  <div>
                    <label className="text-text-secondary text-xs mb-1 block flex items-center gap-1">
                      <Gauge className="w-3 h-3" />
                      Engine Hours
                    </label>
                    <input
                      type="number"
                      defaultValue={engineNum === 1 ? '24.5' : '0.0'}
                      step="0.1"
                      className="w-full bg-background border border-border-color rounded-lg px-3 py-2 text-foreground focus:outline-none focus:border-primary text-sm font-data"
                    />
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Track Database Selection */}
        <div className="carbon-bg border border-border-color rounded-xl p-6">
          <h2 className="text-lg font-racing text-foreground mb-2 flex items-center gap-2">
            <Globe className="w-5 h-5 text-primary" />
            GLOBAL TRACK DATABASE
          </h2>
          <p className="text-text-secondary mb-4 text-xs">
            Select countries to load track maps onto your device.
            <span className="text-primary"> Note: More countries = longer track detection time.</span>
          </p>

          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-2 mb-4">
            {countries.map((country) => (
              <label key={country} className="flex items-center gap-2 cursor-pointer hover:bg-card-bg p-2 rounded transition">
                <input
                  type="checkbox"
                  checked={selectedCountries.includes(country)}
                  onChange={() => toggleCountry(country)}
                  className="w-3 h-3 text-primary bg-background-secondary border-border-color rounded focus:ring-primary accent-primary"
                />
                <span className="text-foreground text-xs">{country}</span>
              </label>
            ))}
          </div>

          <div className="p-3 bg-background-secondary border border-border-color rounded-lg mb-4">
            <div className="text-text-secondary text-xs mb-2">Selected Countries: {selectedCountries.length}</div>
            <div className="flex flex-wrap gap-2">
              {selectedCountries.map((country) => (
                <span key={country} className="bg-primary/20 text-primary px-2 py-1 rounded-lg text-xs font-medium">
                  {country}
                </span>
              ))}
            </div>
          </div>

          <button className="bg-primary hover:bg-primary-hover text-white px-4 py-2 rounded-lg transition flex items-center gap-2 font-racing text-sm">
            <Save className="w-4 h-4" />
            SAVE TRACK SELECTION
          </button>
        </div>
      </div>

    </div>
  );
}
