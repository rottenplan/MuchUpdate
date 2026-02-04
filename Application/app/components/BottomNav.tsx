"use client";

import { Zap, Map, Settings, History } from "lucide-react";
import Link from "next/link";
import { usePathname } from "next/navigation";

export default function BottomNav() {
    return (
        <div className="fixed bottom-0 left-0 right-0 glass-header border-t border-white/5 h-16 flex items-center justify-around pb-safe z-50">
            <NavItem icon={<History />} label="FEED" href="/" />
            <NavItem icon={<Zap />} label="HUB" href="/dashboard" />
            <NavItem icon={<Map />} label="TRACKS" href="/tracks" />
            <NavItem icon={<Settings />} label="SYSTEM" href="/device" />
        </div>
    );
}

function NavItem({ icon, label, href }: { icon: React.ReactNode; label: string; href: string }) {
    const pathname = usePathname();
    const isActive = pathname === href || (href !== "/" && pathname.startsWith(href));

    return (
        <Link
            href={href}
            className={`
                flex flex-col items-center gap-1 px-4 py-2 rounded-lg
                transition-all duration-200
                ${isActive
                    ? "text-primary scale-110"
                    : "text-text-secondary hover:text-foreground hover:scale-105"
                }
            `}
        >
            <div className={`w-6 h-6 ${isActive ? "drop-shadow-[0_0_8px_rgba(220,38,38,0.6)]" : ""}`}>
                {icon}
            </div>
            <span className="text-[10px] font-bold tracking-wide font-racing">{label}</span>
        </Link>
    );
}
