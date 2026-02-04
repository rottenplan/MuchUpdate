'use client';

import { useState, useEffect } from 'react';
import { useRouter } from 'next/navigation';
import Link from 'next/link';
import { Zap, Usb, CheckCircle, AlertCircle, Loader2, ArrowRight, Wifi } from 'lucide-react';

export default function SetupDevicePage() {
    const router = useRouter();
    const [step, setStep] = useState<'connect' | 'detecting' | 'account-setup' | 'wifi-setup' | 'success' | 'error'>('connect');
    const [deviceInfo, setDeviceInfo] = useState<{ sn: string; model: string } | null>(null);
    const [accountData, setAccountData] = useState({ username: '', password: '' });
    const [wifiData, setWifiData] = useState({ ssid: '', password: '' });
    const [errorMsg, setErrorMsg] = useState('');

    // Web Serial API Type Declaration
    // content is intentionally omitted for brevity as it's standard browser API

    const connectDevice = async () => {
        setErrorMsg('');

        // Check if Web Serial API is supported
        if (!('serial' in navigator)) {
            setErrorMsg('Web Serial API not supported in this browser. Please use Chrome or Edge.');
            return;
        }

        try {
            // Request access to the port
            // @ts-ignore
            const port = await navigator.serial.requestPort();

            setStep('detecting');

            // Open the port
            await port.open({ baudRate: 115200 });

            // In a real scenario, we would read from the stream here.
            // For now, we simulate a "handshake" or reading the S/N line.

            // Simulate detection delay
            setTimeout(() => {
                // Mock detected device
                const mockDevice = {
                    sn: 'MR-' + Math.floor(Math.random() * 1000000).toString().padStart(6, '0'),
                    model: 'RaceBox Pro Plus'
                };

                setDeviceInfo(mockDevice);
                setStep('account-setup');

                // Close port cleanup would happen here
                port.close().catch(console.error);

            }, 2000);

        } catch (err: any) {
            console.error('Connection failed:', err);
            // User cancelled or error
            if (err.name === 'NotFoundError') {
                // Just ignore if user cancelled
                return;
            }
            setErrorMsg('Failed to connect to device. Please try again.');
        }
    };

    const handleAccountSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (accountData.username && accountData.password) {
            setStep('wifi-setup');
        }
    };

    const handleWifiSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        setStep('success');
    };

    const handleContinue = () => {
        // Here we would typically save the device association to the user's account via API
        // For now, we just redirect to dashboard
        router.push('/dashboard');
    };

    const handleManualSkip = () => {
        router.push('/dashboard');
    };

    return (
        <div className="min-h-screen bg-background flex flex-col items-center justify-center p-4">
            <div className="w-full max-w-md">

                {/* Header */}
                <div className="text-center mb-8">
                    <h1 className="text-3xl font-racing text-foreground mb-2 tracking-widest italic outline-text">DEVICE SETUP</h1>
                    <p className="text-text-secondary uppercase tracking-tight text-sm">Configure your hardware system</p>
                </div>

                {/* Card */}
                <div className="carbon-bg border border-border-color rounded-xl p-8 shadow-2xl relative overflow-hidden">
                    <div className="absolute top-0 right-0 w-24 h-24 bg-primary/5 -mr-12 -mt-12 rounded-full blur-3xl"></div>

                    {/* Step: Connect */}
                    {step === 'connect' && (
                        <div className="text-center space-y-6">
                            <div className="w-20 h-20 bg-background-secondary rounded-full flex items-center justify-center mx-auto border border-border-color shadow-[0_0_15px_rgba(220,38,38,0.1)]">
                                <Usb className="w-10 h-10 text-primary" />
                            </div>

                            <div>
                                <h2 className="text-xl font-racing text-foreground mb-2">INITIALIZE CONNECTION</h2>
                                <p className="text-text-secondary text-sm">
                                    Plug your device into this computer using a USB cable, then click the button below to sync.
                                </p>
                            </div>

                            {errorMsg && (
                                <div className="bg-warning/10 border border-warning/30 rounded-lg p-3 flex items-center gap-2 text-warning text-sm text-left">
                                    <AlertCircle className="w-5 h-5 flex-shrink-0" />
                                    {errorMsg}
                                </div>
                            )}

                            <button
                                onClick={connectDevice}
                                className="w-full bg-primary hover:bg-primary-hover text-white font-racing py-4 rounded-lg transition shadow-lg hover:shadow-xl flex items-center justify-center gap-2 group"
                            >
                                <Zap className="w-5 h-5 group-hover:scale-125 transition-transform" />
                                CONNECT DEVICE
                            </button>

                            <button
                                onClick={handleManualSkip}
                                className="text-text-secondary hover:text-foreground text-sm underline underline-offset-4"
                            >
                                Skip setup for now
                            </button>
                        </div>
                    )}

                    {/* Step: Detecting */}
                    {step === 'detecting' && (
                        <div className="text-center space-y-8 py-8">
                            <div className="relative">
                                <div className="absolute inset-0 bg-primary/20 blur-xl rounded-full animate-pulse"></div>
                                <Loader2 className="w-16 h-16 text-primary mx-auto animate-spin relative z-10" />
                            </div>

                            <div>
                                <h2 className="text-xl font-racing text-foreground animate-pulse">DETECTING HARDWARE...</h2>
                                <p className="text-text-secondary text-sm mt-2">
                                    Scanning ports for compatible RaceBox hardware.
                                </p>
                            </div>
                        </div>
                    )}

                    {/* Step: Account Setup (NEW) */}
                    {step === 'account-setup' && (
                        <div className="space-y-6">
                            <div className="text-center">
                                <h2 className="text-xl font-racing text-primary mb-1">ACCOUNT SETUP</h2>
                                <p className="text-text-secondary text-xs uppercase">Link device to your profile</p>
                            </div>

                            <form onSubmit={handleAccountSubmit} className="space-y-4">
                                <div className="space-y-1">
                                    <label className="text-[10px] font-bold text-text-secondary tracking-widest pl-1 uppercase">Username</label>
                                    <input
                                        type="text"
                                        value={accountData.username}
                                        onChange={(e) => setAccountData({ ...accountData, username: e.target.value })}
                                        placeholder="Username"
                                        className="w-full bg-background-secondary border border-border-color rounded-lg px-4 py-3 text-foreground placeholder:text-text-secondary/30 focus:outline-none focus:border-primary transition font-data"
                                        required
                                    />
                                </div>

                                <div className="space-y-1">
                                    <label className="text-[10px] font-bold text-text-secondary tracking-widest pl-1 uppercase">Password</label>
                                    <input
                                        type="password"
                                        value={accountData.password}
                                        onChange={(e) => setAccountData({ ...accountData, password: e.target.value })}
                                        placeholder="Password"
                                        className="w-full bg-background-secondary border border-border-color rounded-lg px-4 py-3 text-foreground placeholder:text-text-secondary/30 focus:outline-none focus:border-primary transition font-data"
                                        required
                                    />
                                </div>

                                <button
                                    type="submit"
                                    className="w-full bg-primary hover:bg-primary-hover text-white font-racing py-4 rounded-lg transition shadow-lg hover:shadow-xl mt-4"
                                >
                                    SAVE CREDENTIALS
                                </button>
                            </form>
                        </div>
                    )}

                    {/* Step: WiFi Setup (NEW) */}
                    {step === 'wifi-setup' && (
                        <div className="space-y-6">
                            <div className="text-center">
                                <h2 className="text-xl font-racing text-primary mb-1">CONNECT TO CLOUD</h2>
                                <p className="text-text-secondary text-xs uppercase">Configure WiFi for automatic syncing</p>
                            </div>

                            <form onSubmit={handleWifiSubmit} className="space-y-4">
                                <div className="space-y-1">
                                    <label className="text-[10px] font-bold text-text-secondary tracking-widest pl-1 uppercase">Network SSID</label>
                                    <input
                                        type="text"
                                        value={wifiData.ssid}
                                        onChange={(e) => setWifiData({ ...wifiData, ssid: e.target.value })}
                                        placeholder="WiFi Name"
                                        className="w-full bg-background-secondary border border-border-color rounded-lg px-4 py-3 text-foreground placeholder:text-text-secondary/30 focus:outline-none focus:border-primary transition font-data"
                                        required
                                    />
                                </div>

                                <div className="space-y-1">
                                    <label className="text-[10px] font-bold text-text-secondary tracking-widest pl-1 uppercase">WiFi Password</label>
                                    <input
                                        type="password"
                                        value={wifiData.password}
                                        onChange={(e) => setWifiData({ ...wifiData, password: e.target.value })}
                                        placeholder="Password"
                                        className="w-full bg-background-secondary border border-border-color rounded-lg px-4 py-3 text-foreground placeholder:text-text-secondary/30 focus:outline-none focus:border-primary transition font-data"
                                    />
                                </div>

                                <button
                                    type="submit"
                                    className="w-full bg-primary hover:bg-primary-hover text-white font-racing py-4 rounded-lg transition shadow-lg hover:shadow-xl mt-4"
                                >
                                    FINISH & SYNC
                                </button>

                                <button
                                    type="button"
                                    onClick={() => setStep('success')}
                                    className="w-full text-text-secondary hover:text-foreground text-xs uppercase tracking-widest font-racing transition"
                                >
                                    SKIP WIFI SETUP
                                </button>
                            </form>
                        </div>
                    )}
                    {step === 'success' && deviceInfo && (
                        <div className="text-center space-y-6">
                            <div className="w-20 h-20 bg-highlight/10 rounded-full flex items-center justify-center mx-auto border border-highlight/30 shadow-[0_0_20px_rgba(16,185,129,0.1)]">
                                <CheckCircle className="w-10 h-10 text-highlight" />
                            </div>

                            <div>
                                <h2 className="text-xl font-racing text-highlight mb-1">SYNC COMPLETE</h2>
                                <p className="text-text-secondary text-sm">Hardware is now linked to <span className="text-foreground font-bold">{accountData.username}</span></p>
                            </div>

                            <div className="bg-background-secondary/50 border border-border-color rounded-lg p-4 text-left backdrop-blur-sm">
                                <div className="flex justify-between items-center mb-2 border-b border-border-color/30 pb-2">
                                    <span className="text-text-secondary text-[10px] font-bold tracking-widest uppercase">System Model</span>
                                    <span className="text-foreground font-racing text-sm tracking-wide">{deviceInfo.model}</span>
                                </div>
                                <div className="flex justify-between items-center pt-1">
                                    <span className="text-text-secondary text-[10px] font-bold tracking-widest uppercase">Serial Index</span>
                                    <span className="text-primary font-data font-bold tracking-widest text-sm">{deviceInfo.sn}</span>
                                </div>
                            </div>

                            <button
                                onClick={handleContinue}
                                className="w-full bg-highlight hover:bg-highlight-hover text-white font-racing py-4 rounded-lg transition shadow-lg hover:shadow-xl flex items-center justify-center gap-2 group"
                            >
                                CONTINUE TO DASHBOARD
                                <ArrowRight className="w-5 h-5 group-hover:translate-x-1 transition-transform" />
                            </button>
                        </div>
                    )}

                </div>

                {/* Footer */}
                <div className="mt-8 text-center">
                    <p className="text-text-secondary text-xs">
                        Having trouble? <Link href="/help" className="text-primary hover:underline">View connection guide</Link>
                    </p>
                </div>

            </div>
        </div>
    );
}
