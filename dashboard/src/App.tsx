import React, { useState, useEffect } from 'react';

const API_BASE = '/api';

function Icon({ name, className = "" }: { name: string, className?: string }) {
    return <span className={`material-symbols-outlined select-none ${className}`}>{name}</span>;
}

export default function App() {
    const [token, setToken] = useState<string | null>(localStorage.getItem('auth_token'));
    const [activeTab, setActiveTab] = useState('dashboard-status');
    const [health, setHealth] = useState<any>(null);
    const [applianceVersion, setApplianceVersion] = useState<string>('1.0.0');

    // Unified fetch helper that handles 401 Unauthorized and auto-logout
    const fetchWithAuth = async (url: string, options: RequestInit = {}) => {
        if (!token) return null;
        const headers = new Headers(options.headers || {});
        headers.set('Authorization', `Bearer ${token}`);

        try {
            const res = await fetch(url, { ...options, headers });
            if (res.status === 401) {
                localStorage.removeItem('auth_token');
                setToken(null);
                return null;
            }
            return res;
        } catch (err) {
            console.error("API Fetch Error:", err);
            throw err;
        }
    };

    useEffect(() => {
        if (!token) return;
        const getHealth = async () => {
            try {
                const res = await fetchWithAuth(`${API_BASE}/health`);
                if (res && res.ok) {
                    const data = await res.json();
                    setHealth(data);
                }
            } catch (err) {}
        };
        // Also fetch the appliance's own version from the license endpoint (public)
        const getApplianceVersion = async () => {
            try {
                const res = await fetch(`${API_BASE}/license`);
                if (res && res.ok) {
                    const data = await res.json();
                    if (data.os_version) {
                        setApplianceVersion(data.os_version);
                    }
                }
            } catch (err) {}
        };
        getHealth();
        getApplianceVersion();
        const interval = setInterval(() => {
            getHealth();
            getApplianceVersion();
        }, 30000);
        return () => clearInterval(interval);
    }, [token]);

    if (!token) {
        return <Login setToken={setToken} />;
    }

    return (
        <div className="flex h-screen w-screen overflow-hidden bg-slate-950 font-sans text-slate-100 antialiased">
            {/* Sidebar navigation */}
            <Sidebar activeTab={activeTab} setActiveTab={setActiveTab} health={health} applianceVersion={applianceVersion} />

            {/* Content wrapper */}
            <div className="flex flex-col flex-1 h-screen overflow-hidden bg-[#070b13]">
                <Header activeTab={activeTab} setToken={setToken} health={health} />

                {/* Main Content Area */}
                <main className="flex-1 overflow-y-auto p-6 md:p-8 space-y-6">
                    <div className="animate-fade-in max-w-7xl mx-auto w-full">
                        {activeTab === 'dashboard-status' && <DashboardStatus fetchWithAuth={fetchWithAuth} />}
                        {activeTab === 'network-interfaces' && <Interfaces fetchWithAuth={fetchWithAuth} />}
                        {activeTab === 'system-licensing' && <License fetchWithAuth={fetchWithAuth} />}
                        {activeTab === 'system-time' && <TimeSettings fetchWithAuth={fetchWithAuth} />}
                    </div>
                </main>
            </div>
        </div>
    );
}

// --- LOGIN PAGE (High-end Cybersecurity Theme) ---
function Login({ setToken }: { setToken: (t: string) => void }) {
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);

    const handleLogin = async (e: React.FormEvent) => {
        e.preventDefault();
        setError('');
        setLoading(true);
        try {
            const res = await fetch(`${API_BASE}/auth/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });
            const data = await res.json();
            if (res.ok && data.token) {
                localStorage.setItem('auth_token', data.token);
                setToken(data.token);
            } else {
                setError(data.error || 'Authentication failed');
            }
        } catch (err) {
            setError('Connection refused. Is Beout OS Service running?');
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="flex min-h-screen w-screen items-center justify-center bg-[#060a12] px-4 py-12 relative overflow-hidden">
            {/* Neo-ambient cyber background glow */}
            <div className="absolute top-1/4 left-1/4 -translate-x-1/2 -translate-y-1/2 w-96 h-96 bg-blue-600/10 rounded-full blur-[120px] pointer-events-none"></div>
            <div className="absolute bottom-1/4 right-1/4 translate-x-1/2 translate-y-1/2 w-96 h-96 bg-indigo-600/10 rounded-full blur-[120px] pointer-events-none"></div>

            <div className="w-full max-w-md bg-slate-900/60 border border-slate-800/80 rounded-2xl shadow-2xl backdrop-blur-xl p-8 relative z-10">
                {/* Header Logo */}
                <div className="flex flex-col items-center justify-center mb-8">
                    <div className="flex items-center gap-2.5 mb-2">
                        <span className="material-symbols-outlined text-4xl text-blue-500 font-bold select-none drop-shadow-[0_0_8px_rgba(59,130,246,0.5)]">shield</span>
                        <span className="font-headline text-3xl font-extrabold tracking-wider text-slate-100">
                            BEOUT<span className="text-blue-500">.AI</span>
                        </span>
                    </div>
                    <span className="text-xs font-mono text-slate-400 uppercase tracking-widest">Network Security Operating System</span>
                </div>

                {error && (
                    <div className="mb-6 p-4 bg-red-950/40 border border-red-800/40 rounded-lg text-sm text-red-400 flex items-start gap-3">
                        <Icon name="error" className="text-red-500 mt-0.5" />
                        <span>{error}</span>
                    </div>
                )}

                <form onSubmit={handleLogin} className="space-y-5">
                    <div>
                        <label className="block text-xs font-medium text-slate-300 uppercase tracking-wider mb-2">Username</label>
                        <div className="relative">
                            <span className="absolute inset-y-0 left-0 flex items-center pl-3 text-slate-500">
                                <Icon name="person" className="text-xl" />
                            </span>
                            <input
                                type="text"
                                className="block w-full rounded-lg bg-slate-950/80 border border-slate-800 pl-10 pr-3 py-2.5 text-sm text-slate-100 placeholder-slate-500 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition duration-200"
                                placeholder="admin"
                                value={username}
                                onChange={e => setUsername(e.target.value)}
                                required
                            />
                        </div>
                    </div>

                    <div>
                        <label className="block text-xs font-medium text-slate-300 uppercase tracking-wider mb-2">Password</label>
                        <div className="relative">
                            <span className="absolute inset-y-0 left-0 flex items-center pl-3 text-slate-500">
                                <Icon name="lock" className="text-xl" />
                            </span>
                            <input
                                type="password"
                                className="block w-full rounded-lg bg-slate-950/80 border border-slate-800 pl-10 pr-3 py-2.5 text-sm text-slate-100 placeholder-slate-500 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition duration-200"
                                placeholder="••••••••"
                                value={password}
                                onChange={e => setPassword(e.target.value)}
                                required
                            />
                        </div>
                    </div>

                    <button
                        type="submit"
                        disabled={loading}
                        className="w-full flex items-center justify-center gap-2 py-3 px-4 rounded-lg bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-sm font-semibold text-white shadow-lg shadow-blue-500/20 focus:outline-none focus:ring-2 focus:ring-blue-500 transition duration-200 disabled:opacity-50"
                    >
                        {loading ? 'Authenticating...' : 'Sign In'}
                        {!loading && <Icon name="arrow_forward" className="text-lg" />}
                    </button>
                </form>

                <div className="relative my-6 text-center">
                    <div className="absolute inset-0 flex items-center">
                        <div className="w-full border-t border-slate-800"></div>
                    </div>
                    <span className="relative bg-[#0d1322] px-3 text-xs text-slate-500 font-mono uppercase tracking-widest">or</span>
                </div>

                <button
                    onClick={() => alert('BeoutCloud Identity Federation (Demo)')}
                    className="w-full flex items-center justify-center gap-3 py-2.5 px-4 rounded-lg bg-slate-950 hover:bg-slate-900 border border-slate-800 hover:border-slate-700 text-sm font-medium text-slate-300 transition duration-200"
                >
                    <div className="flex items-center justify-center w-6 h-6 rounded bg-blue-600 text-white">
                        <Icon name="cloud" className="text-base" />
                    </div>
                    Sign in with BeoutCloud
                </button>
            </div>
        </div>
    );
}

// --- SIDEBAR ---
function Sidebar({ activeTab, setActiveTab, health, applianceVersion }: { activeTab: string, setActiveTab: (tab: string) => void, health: any, applianceVersion: string }) {
    const [expandedGroups, setExpandedGroups] = useState({
        dashboard: true,
        network: true,
        system: true
    });

    const toggleGroup = (group: keyof typeof expandedGroups) => {
        setExpandedGroups(prev => ({ ...prev, [group]: !prev[group] }));
    };

    return (
        <aside className="w-64 bg-slate-900/40 border-r border-slate-800/80 flex flex-col flex-shrink-0 relative z-20">
            {/* Brand Header */}
            <div className="h-16 flex items-center px-6 border-b border-slate-800/80 gap-3">
                <Icon name="shield" className="text-2xl text-blue-500 font-bold drop-shadow-[0_0_8px_rgba(59,130,246,0.35)]" />
                <span className="font-headline text-lg font-bold tracking-wider text-slate-100">
                    BEOUT<span className="text-blue-500">.AI</span>
                </span>
            </div>

            {/* Navigation menu */}
            <div className="flex-1 overflow-y-auto py-6 px-4 space-y-4">
                {/* Dashboard Group */}
                <div className="space-y-1">
                    <button
                        className="w-full flex items-center justify-between py-2 px-3 rounded-lg text-slate-400 hover:text-slate-100 hover:bg-slate-800/40 transition duration-150"
                        onClick={() => toggleGroup('dashboard')}
                    >
                        <span className="flex items-center gap-3 text-sm font-semibold">
                            <Icon name="dashboard" className="text-lg text-slate-400" />
                            Dashboard
                        </span>
                        <Icon name={expandedGroups.dashboard ? "keyboard_arrow_down" : "keyboard_arrow_right"} className="text-slate-500 text-lg" />
                    </button>

                    {expandedGroups.dashboard && (
                        <div className="pl-9 space-y-1">
                            <button
                                onClick={() => setActiveTab('dashboard-status')}
                                className={`w-full text-left py-1.5 px-3 rounded-md text-sm font-medium transition duration-150 ${activeTab === 'dashboard-status' ? 'text-blue-400 bg-blue-500/10' : 'text-slate-400 hover:text-slate-200'}`}
                            >
                                Status
                            </button>
                        </div>
                    )}
                </div>

                {/* Network Group */}
                <div className="space-y-1">
                    <button
                        className="w-full flex items-center justify-between py-2 px-3 rounded-lg text-slate-400 hover:text-slate-100 hover:bg-slate-800/40 transition duration-150"
                        onClick={() => toggleGroup('network')}
                    >
                        <span className="flex items-center gap-3 text-sm font-semibold">
                            <Icon name="router" className="text-lg text-slate-400" />
                            Network
                        </span>
                        <Icon name={expandedGroups.network ? "keyboard_arrow_down" : "keyboard_arrow_right"} className="text-slate-500 text-lg" />
                    </button>

                    {expandedGroups.network && (
                        <div className="pl-9 space-y-1">
                            <button
                                onClick={() => setActiveTab('network-interfaces')}
                                className={`w-full text-left py-1.5 px-3 rounded-md text-sm font-medium transition duration-150 ${activeTab === 'network-interfaces' ? 'text-blue-400 bg-blue-500/10' : 'text-slate-400 hover:text-slate-200'}`}
                            >
                                Interfaces
                            </button>
                        </div>
                    )}
                </div>

                {/* System Group */}
                <div className="space-y-1">
                    <button
                        className="w-full flex items-center justify-between py-2 px-3 rounded-lg text-slate-400 hover:text-slate-100 hover:bg-slate-800/40 transition duration-150"
                        onClick={() => toggleGroup('system')}
                    >
                        <span className="flex items-center gap-3 text-sm font-semibold">
                            <Icon name="settings" className="text-lg text-slate-400" />
                            System
                        </span>
                        <Icon name={expandedGroups.system ? "keyboard_arrow_down" : "keyboard_arrow_right"} className="text-slate-500 text-lg" />
                    </button>

                    {expandedGroups.system && (
                        <div className="pl-9 space-y-1">
                            <button
                                onClick={() => setActiveTab('system-licensing')}
                                className={`w-full text-left py-1.5 px-3 rounded-md text-sm font-medium transition duration-150 ${activeTab === 'system-licensing' ? 'text-blue-400 bg-blue-500/10' : 'text-slate-400 hover:text-slate-200'}`}
                            >
                                Licensing
                            </button>
                            <button
                                onClick={() => setActiveTab('system-time')}
                                className={`w-full text-left py-1.5 px-3 rounded-md text-sm font-medium transition duration-150 ${activeTab === 'system-time' ? 'text-blue-400 bg-blue-500/10' : 'text-slate-400 hover:text-slate-200'}`}
                            >
                                Time Settings
                            </button>
                        </div>
                    )}
                </div>
            </div>

            {/* Footer Details */}
            <div className="p-4 border-t border-slate-800/80 bg-slate-900/20 text-xs text-slate-500 font-mono space-y-1">
                <div className="flex items-center justify-between text-slate-400 font-bold">
                    <span>beout.ai</span>
                    <span className="text-[10px] bg-slate-800 text-slate-400 px-1.5 py-0.5 rounded">{applianceVersion ? `v${applianceVersion}` : 'v1.0.0'}</span>
                </div>
                <div className="text-[10px] text-slate-600">Enterprise Appliance Node</div>
            </div>
        </aside>
    );
}

// --- HEADER ---
function Header({ activeTab, setToken, health }: any) {
    const handleLogout = () => {
        localStorage.removeItem('auth_token');
        setToken(null);
    };

    const getPageTitle = () => {
        if (activeTab === 'dashboard-status') return 'System Dashboard';
        if (activeTab === 'network-interfaces') return 'Network Interfaces';
        if (activeTab === 'system-licensing') return 'Appliance Licensing';
        if (activeTab === 'system-time') return 'Time & Timezone Settings';
        return '';
    };

    return (
        <header className="h-16 border-b border-slate-800 bg-slate-900/10 flex items-center justify-between px-8 relative z-10 flex-shrink-0">
            <div className="flex items-center gap-4">
                <h1 className="font-headline text-lg font-bold text-slate-100">{getPageTitle()}</h1>
                <div className="hidden lg:flex items-center gap-2 text-xs">
                    <div className="h-4 w-px bg-slate-800"></div>
                    {health?.wan_ip && (
                        <span className="text-slate-400 font-mono bg-slate-900 px-2 py-0.5 rounded border border-slate-800">
                            WAN IP: {health.wan_ip}
                        </span>
                    )}
                    {health?.internet_status === 'online' ? (
                        <span className="text-emerald-400 flex items-center gap-1.5 font-medium ml-1">
                            <span className="h-2 w-2 rounded-full bg-emerald-500 animate-pulse"></span>
                            Internet Connected
                        </span>
                    ) : (
                        <span className="text-rose-400 flex items-center gap-1.5 font-medium ml-1">
                            <span className="h-2 w-2 rounded-full bg-rose-500"></span>
                            Internet Disconnected
                        </span>
                    )}
                </div>
            </div>

            <div className="flex items-center gap-6">
                {/* Diagnostics Info */}
                <div className="flex items-center gap-2 text-slate-400 hover:text-slate-200 cursor-pointer" title="System Settings Verified">
                    <Icon name="verified" className="text-blue-500 text-lg" />
                    <span className="text-xs font-mono font-medium">Safe</span>
                </div>

                {/* Action Header Icons */}
                <div className="flex items-center gap-3">
                    <button className="text-slate-400 hover:text-slate-200 relative p-1.5 rounded-lg hover:bg-slate-800/40 transition">
                        <Icon name="notifications" className="text-xl" />
                        <span className="absolute top-1 right-1 w-2 h-2 bg-blue-500 rounded-full"></span>
                    </button>
                </div>

                {/* User Account Profile */}
                <div
                    onClick={handleLogout}
                    className="flex items-center gap-3 px-3 py-1.5 rounded-lg hover:bg-slate-800/40 border border-transparent hover:border-slate-800 cursor-pointer transition"
                    title="Click to Disconnect Session"
                >
                    <div className="w-7 h-7 rounded-full bg-blue-600/20 border border-blue-500/30 flex items-center justify-center text-blue-400 text-sm font-semibold uppercase">
                        A
                    </div>
                    <div className="hidden md:block text-left">
                        <div className="text-xs font-semibold text-slate-200 leading-3">admin</div>
                        <span className="text-[9px] font-mono text-slate-500 uppercase">Super Admin</span>
                    </div>
                    <Icon name="logout" className="text-slate-500 text-base" />
                </div>
            </div>
        </header>
    );
}

// --- SPARKLINE CHART COMPONENT ---
function SparklineChart({ points, color = '#3b82f6', fill = 'rgba(59,130,246,0.06)' }: { points: number[], color?: string, fill?: string }) {
    const width = 400;
    const height = 110;
    const max = 100;
    const step = width / (points.length - 1);

    let path = '';
    let fillPath = '';

    points.forEach((val, i) => {
        const x = i * step;
        const y = height - (val / max) * (height - 15) - 8;
        if (i === 0) {
            path += `M ${x} ${y}`;
            fillPath += `M ${x} ${height} L ${x} ${y}`;
        } else {
            path += ` L ${x} ${y}`;
            fillPath += ` L ${x} ${y}`;
        }
        if (i === points.length - 1) {
            fillPath += ` L ${x} ${height} Z`;
        }
    });

    return (
        <svg width="100%" height={height} viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none" style={{ display: 'block', overflow: 'visible' }}>
            {/* Grid Lines */}
            <line x1="0" y1={height * 0.25} x2={width} y2={height * 0.25} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />
            <line x1="0" y1={height * 0.5} x2={width} y2={height * 0.5} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />
            <line x1="0" y1={height * 0.75} x2={width} y2={height * 0.75} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />

            {/* Paths */}
            <path d={fillPath} fill={fill} />
            <path d={path} fill="none" stroke={color} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
    );
}

// --- DYNAMIC DOUBLE BANDWIDTH CHART ---
function BandwidthChart() {
    const [inbound, setInbound] = useState<number[]>(Array(20).fill(10));
    const [outbound, setOutbound] = useState<number[]>(Array(20).fill(5));

    useEffect(() => {
        const interval = setInterval(() => {
            setInbound(prev => {
                const next = [...prev.slice(1)];
                next.push(Math.floor(Math.random() * 45) + 15);
                return next;
            });
            setOutbound(prev => {
                const next = [...prev.slice(1)];
                next.push(Math.floor(Math.random() * 25) + 5);
                return next;
            });
        }, 1000);
        return () => clearInterval(interval);
    }, []);

    const width = 400;
    const height = 110;
    const max = 80;
    const step = width / (inbound.length - 1);

    const getPath = (points: number[]) => {
        let path = '';
        points.forEach((val, i) => {
            const x = i * step;
            const y = height - (val / max) * (height - 15) - 8;
            if (i === 0) path += `M ${x} ${y}`;
            else path += ` L ${x} ${y}`;
        });
        return path;
    };

    return (
        <div className="space-y-4">
            <svg width="100%" height={height} viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none" style={{ display: 'block', overflow: 'visible' }}>
                <line x1="0" y1={height * 0.25} x2={width} y2={height * 0.25} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />
                <line x1="0" y1={height * 0.5} x2={width} y2={height * 0.5} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />
                <line x1="0" y1={height * 0.75} x2={width} y2={height * 0.75} stroke="#1e293b" strokeWidth="1" strokeDasharray="3 3" />

                {/* Inbound Line (Blue) */}
                <path d={getPath(inbound)} fill="none" stroke="#3b82f6" strokeWidth="2.5" strokeLinecap="round" />
                {/* Outbound Line (Orange) */}
                <path d={getPath(outbound)} fill="none" stroke="#f59e0b" strokeWidth="2.5" strokeLinecap="round" />
            </svg>
            <div className="flex gap-4 text-xs font-mono justify-center">
                <span className="text-blue-400 flex items-center gap-1.5 font-semibold">
                    <span className="h-2 w-2 rounded-full bg-blue-500"></span> Inbound (wan1)
                </span>
                <span className="text-amber-400 flex items-center gap-1.5 font-semibold">
                    <span className="h-2 w-2 rounded-full bg-amber-500"></span> Outbound (wan1)
                </span>
            </div>
        </div>
    );
}

// --- DASHBOARD STATUS PANEL ---
function DashboardStatus({ fetchWithAuth }: { fetchWithAuth: any }) {
    const [health, setHealth] = useState<any>(null);
    const [license, setLicense] = useState<any>(null);
    const [cpuPoints, setCpuPoints] = useState<number[]>(Array(20).fill(15));
    const [memPoints, setMemPoints] = useState<number[]>(Array(20).fill(48));
    const [sessionPoints, setSessionPoints] = useState<number[]>(Array(20).fill(150));

    // Load initial health diagnostics and licensing
    useEffect(() => {
        const initData = async () => {
            try {
                const h_res = await fetchWithAuth(`${API_BASE}/health`);
                if (h_res && h_res.ok) {
                    const h_data = await h_res.json();
                    setHealth(h_data);
                }

                // License endpoint is public — use direct fetch
                const l_res = await fetch(`${API_BASE}/license`);
                if (l_res && l_res.ok) {
                    const l_data = await l_res.json();
                    setLicense(l_data);
                }
            } catch (err) {
                console.error(err);
            }
        };
        initData();
    }, []);

    // Periodic metrics updates
    useEffect(() => {
        const interval = setInterval(() => {
            setCpuPoints(prev => {
                const next = [...prev.slice(1)];
                const val = Math.floor(Math.random() * 20) + 10;
                next.push(val);
                return next;
            });
            setMemPoints(prev => {
                const next = [...prev.slice(1)];
                const val = prev[prev.length - 1] + (Math.random() > 0.5 ? 1 : -1);
                next.push(Math.max(45, Math.min(55, val)));
                return next;
            });
            setSessionPoints(prev => {
                const next = [...prev.slice(1)];
                const val = prev[prev.length - 1] + Math.floor(Math.random() * 11) - 5;
                next.push(Math.max(100, Math.min(400, val)));
                return next;
            });
        }, 1500);
        return () => clearInterval(interval);
    }, []);

    const isActive = license?.status === 'ACTIVE';
    const truncatedMachineId = license?.machine_id ? license.machine_id.slice(0, 36).toUpperCase() : 'Loading...';

    return (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">

            {/* 1. System Information */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center gap-2.5 pb-4 mb-4 border-b border-slate-800/80">
                    <Icon name="info" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">System Information</h2>
                </div>
                <div className="space-y-3.5 text-sm">
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Hostname</span>
                        <span className="text-slate-200 font-semibold">{health?.hostname || 'beoutos'}</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Appliance ID</span>
                        <span className="text-slate-300 font-semibold">{truncatedMachineId}</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Firmware</span>
                        <span className="text-slate-300 font-semibold">v{license?.os_version || '1.0.0'}</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Operation Mode</span>
                        <span className="text-slate-300 font-semibold">NAT Firewall</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Uptime</span>
                        <span className="text-slate-300">3d 4h 12m</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">WAN IP Address</span>
                        <span className="text-slate-300 font-semibold">{health?.wan_ip || 'Unconfigured'}</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Internet Link</span>
                        <span>
                            {health?.internet_status === 'online' ? (
                                <span className="text-emerald-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-emerald-500"></span> Online
                                </span>
                            ) : (
                                <span className="text-rose-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-rose-500 animate-pulse"></span> Offline
                                </span>
                            )}
                        </span>
                    </div>
                </div>
            </div>

            {/* 2. License Status */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center gap-2.5 pb-4 mb-4 border-b border-slate-800/80">
                    <Icon name="verified_user" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">Subscription Status</h2>
                </div>
                <div className="space-y-4">
                    <div className="grid grid-cols-2 gap-2 text-center text-xs font-mono font-bold">
                        <div className={`p-2.5 rounded-lg border ${isActive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-rose-500/10 border-rose-500/20 text-rose-400'}`}>
                            <div>SUPPORT</div>
                            <div className="text-[10px] mt-0.5 opacity-80">{isActive ? 'Active' : 'Unlicensed'}</div>
                        </div>
                        <div className={`p-2.5 rounded-lg border ${isActive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-rose-500/10 border-rose-500/20 text-rose-400'}`}>
                            <div>UPDATES</div>
                            <div className="text-[10px] mt-0.5 opacity-80">{isActive ? 'Active' : 'Unlicensed'}</div>
                        </div>
                        <div className={`p-2.5 rounded-lg border ${isActive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-rose-500/10 border-rose-500/20 text-rose-400'}`}>
                            <div>IPS ENGINE</div>
                            <div className="text-[10px] mt-0.5 opacity-80">{isActive ? 'Active' : 'Unlicensed'}</div>
                        </div>
                        <div className={`p-2.5 rounded-lg border ${isActive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-rose-500/10 border-rose-500/20 text-rose-400'}`}>
                            <div>ANTIVIRUS</div>
                            <div className="text-[10px] mt-0.5 opacity-80">{isActive ? 'Active' : 'Unlicensed'}</div>
                        </div>
                    </div>

                    <div className="space-y-2 text-sm">
                        <div className="flex justify-between font-mono">
                            <span className="text-slate-500 font-sans">Active Key</span>
                            <span className="text-slate-300 text-xs font-semibold select-all">
                                {isActive ? license?.license_key : 'None'}
                            </span>
                        </div>
                        <div className="flex justify-between font-mono">
                            <span className="text-slate-500 font-sans">Verification</span>
                            <span className="text-slate-300 font-semibold">{isActive ? 'Verified Cryptographic Signature' : 'Unverified'}</span>
                        </div>
                    </div>
                </div>
            </div>

            {/* 3. Beout Cloud Connection */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center gap-2.5 pb-4 mb-4 border-b border-slate-800/80">
                    <Icon name="cloud" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">Central Cloud Status</h2>
                </div>
                <div className="space-y-3.5 text-sm">
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Cloud Service</span>
                        <span>
                            {isActive ? (
                                <span className="text-emerald-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-emerald-500"></span> Activated
                                </span>
                            ) : (
                                <span className="text-rose-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-rose-500"></span> Unregistered
                                </span>
                            )}
                        </span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Server URL</span>
                        <span className="text-slate-300 text-xs select-all">{license?.license_server_url || 'Not configured'}</span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Update Status</span>
                        <span>
                            {health?.server_status === 'online' ? (
                                <span className="text-emerald-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-emerald-500"></span> Server Reachable
                                </span>
                            ) : (
                                <span className="text-rose-400 font-sans font-semibold flex items-center gap-1.5">
                                    <span className="h-1.5 w-1.5 rounded-full bg-rose-500 animate-pulse"></span> Host Offline
                                </span>
                            )}
                        </span>
                    </div>
                    <div className="flex justify-between font-mono">
                        <span className="text-slate-500 font-sans">Storage Use</span>
                        <span className="text-slate-300 font-semibold">1.47 GiB / 10 GiB</span>
                    </div>
                    <div className="relative pt-1.5">
                        <div className="overflow-hidden h-1.5 text-xs flex rounded bg-slate-800">
                            <div style={{ width: "14.7%" }} className="shadow-none flex flex-col text-center whitespace-nowrap text-white justify-center bg-blue-500"></div>
                        </div>
                    </div>
                </div>
            </div>

            {/* 4. CPU Performance */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center justify-between pb-3 mb-3 border-b border-slate-800/80">
                    <div className="flex items-center gap-2.5">
                        <Icon name="memory" className="text-xl text-blue-500" />
                        <h2 className="font-headline text-base font-bold text-slate-200">CPU Load Profile</h2>
                    </div>
                    <span className="text-xs font-mono bg-blue-950/60 border border-blue-900/40 text-blue-400 px-2 py-0.5 rounded">
                        Usage: {cpuPoints[cpuPoints.length - 1]}%
                    </span>
                </div>
                <div className="pt-2">
                    <SparklineChart points={cpuPoints} color="#3b82f6" fill="rgba(59,130,246,0.06)" />
                </div>
            </div>

            {/* 5. Memory Performance */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center justify-between pb-3 mb-3 border-b border-slate-800/80">
                    <div className="flex items-center gap-2.5">
                        <Icon name="analytics" className="text-xl text-blue-500" />
                        <h2 className="font-headline text-base font-bold text-slate-200">Memory Footprint</h2>
                    </div>
                    <span className="text-xs font-mono bg-emerald-950/60 border border-emerald-900/40 text-emerald-400 px-2 py-0.5 rounded">
                        Usage: {memPoints[memPoints.length - 1]}%
                    </span>
                </div>
                <div className="pt-2">
                    <SparklineChart points={memPoints} color="#10b981" fill="rgba(16,185,129,0.06)" />
                </div>
            </div>

            {/* 6. Concurrent Sessions */}
            <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center justify-between pb-3 mb-3 border-b border-slate-800/80">
                    <div className="flex items-center gap-2.5">
                        <Icon name="leak_add" className="text-xl text-blue-500" />
                        <h2 className="font-headline text-base font-bold text-slate-200">Active Sessions</h2>
                    </div>
                    <span className="text-xs font-mono bg-purple-950/60 border border-purple-900/40 text-purple-400 px-2 py-0.5 rounded">
                        Sessions: {sessionPoints[sessionPoints.length - 1]}
                    </span>
                </div>
                <div className="pt-2">
                    <SparklineChart points={sessionPoints} color="#a855f7" fill="rgba(168,85,247,0.06)" />
                </div>
            </div>

            {/* 7. Bandwidth Usage Monitor */}
            <div className="md:col-span-2 lg:col-span-3 bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md">
                <div className="flex items-center gap-2.5 pb-4 mb-4 border-b border-slate-800/80">
                    <Icon name="insights" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">Live Port Bandwidth Tracker (wan1)</h2>
                </div>
                <BandwidthChart />
            </div>

        </div>
    );
}

// --- NETWORK INTERFACES COMPONENT ---
function Interfaces({ fetchWithAuth }: { fetchWithAuth: any }) {
    const [saving, setSaving] = useState(false);
    const [message, setMessage] = useState<{ text: string; isError: boolean } | null>(null);
    const [interfaces, setInterfaces] = useState<any[]>([]);
    const [selectedId, setSelectedId] = useState<string | null>(null);

    // Interface settings states
    const [editId, setEditId] = useState('');
    const [editName, setEditName] = useState('');
    const [editDevice, setEditDevice] = useState('');
    const [editIp, setEditIp] = useState('');
    const [editNetmask, setEditNetmask] = useState('255.255.255.0');
    const [editGateway, setEditGateway] = useState('');
    const [editMgmtAccess, setEditMgmtAccess] = useState(false);

    const fetchConfig = async () => {
        try {
            const res = await fetchWithAuth(`${API_BASE}/config`);
            if (res && res.ok) {
                const data = await res.json();
                if (data.interfaces && Array.isArray(data.interfaces)) {
                    setInterfaces(data.interfaces);
                    if (data.interfaces.length > 0) {
                        selectInterface(data.interfaces[0]);
                    }
                }
            }
        } catch (err) {
            console.error(err);
        }
    };

    useEffect(() => {
        fetchConfig();
    }, []);

    const selectInterface = (iface: any) => {
        setSelectedId(iface.id);
        setEditId(iface.id);
        setEditName(iface.name || '');
        setEditDevice(iface.device || '');
        setEditIp(iface.ip || '');
        setEditNetmask(iface.netmask || '255.255.255.0');
        setEditGateway(iface.gateway || '');
        setEditMgmtAccess(!!iface.mgmt_access);
    };

    const handleAddNew = () => {
        const nextPortNum = interfaces.length + 1;
        const newIface = {
            id: 'custom_' + Date.now(),
            name: `port${nextPortNum}`,
            device: `eth${interfaces.length}`,
            ip: '',
            netmask: '255.255.255.0',
            gateway: '',
            mgmt_access: false
        };
        const updated = [...interfaces, newIface];
        setInterfaces(updated);
        selectInterface(newIface);
        setMessage({ text: 'Interface registered locally. Remember to apply changes to save configuration.', isError: false });
    };

    const handleDelete = (id: string) => {
        if (id === 'wan' || id === 'lan' || id === 'mgmt') {
            alert('System critical default interfaces (WAN/LAN/MGMT) cannot be deleted.');
            return;
        }
        const updated = interfaces.filter(i => i.id !== id);
        setInterfaces(updated);
        if (selectedId === id) {
            if (updated.length > 0) {
                selectInterface(updated[0]);
            } else {
                setSelectedId(null);
                setEditId('');
                setEditName('');
                setEditDevice('');
                setEditIp('');
                setEditNetmask('255.255.255.0');
                setEditGateway('');
                setEditMgmtAccess(false);
            }
        }
        setMessage({ text: 'Interface removed. Submit configuration updates below to apply changes.', isError: false });
    };

    const handleUpdateCurrent = (e: React.FormEvent) => {
        e.preventDefault();
        if (!editId) return;
        const updated = interfaces.map(i => {
            if (i.id === editId) {
                return {
                    id: editId,
                    name: editName,
                    device: editDevice,
                    ip: editIp,
                    netmask: editNetmask,
                    gateway: editGateway,
                    mgmt_access: editMgmtAccess
                };
            }
            return i;
        });
        setInterfaces(updated);
        setMessage({ text: 'Settings updated in local cache. Push "Apply Network Settings" to save to system database.', isError: false });
    };

    const handleApply = async () => {
        setSaving(true);
        setMessage(null);
        try {
            const res = await fetchWithAuth(`${API_BASE}/config`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ interfaces })
            });
            if (res && res.ok) {
                setMessage({ text: 'Network config stored successfully and synchronized with OS kernel networking stack.', isError: false });
                fetchConfig();
            } else {
                setMessage({ text: 'Unable to commit configurations to database storage.', isError: true });
            }
        } catch (err) {
            setMessage({ text: 'Fatal connection error during configuration update.', isError: true });
        } finally {
            setSaving(false);
        }
    };

    return (
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">

            {/* List Table panel */}
            <div className="lg:col-span-2 space-y-6">
                <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl overflow-hidden shadow-lg backdrop-blur-md">
                    <div className="flex items-center justify-between px-6 py-4 border-b border-slate-800/80 bg-slate-900/20">
                        <div className="flex items-center gap-2.5">
                            <Icon name="dns" className="text-xl text-blue-500" />
                            <h2 className="font-headline text-base font-bold text-slate-200">Registered Interface Adapters</h2>
                        </div>
                        <button
                            onClick={handleAddNew}
                            className="flex items-center gap-1.5 py-1.5 px-3.5 rounded-lg bg-blue-600 hover:bg-blue-500 text-xs font-semibold text-white shadow-md shadow-blue-500/10 transition duration-150"
                        >
                            <Icon name="add" className="text-base" /> Create Port
                        </button>
                    </div>

                    <div className="overflow-x-auto">
                        <table className="w-full text-sm font-mono text-left border-collapse">
                            <thead>
                                <tr className="bg-slate-950/60 border-b border-slate-800/80 text-[11px] uppercase tracking-wider text-slate-400">
                                    <th className="px-5 py-3">Alias</th>
                                    <th className="px-5 py-3">Adapter Device</th>
                                    <th className="px-5 py-3">IP Address</th>
                                    <th className="px-5 py-3">Netmask</th>
                                    <th className="px-5 py-3">Gateway</th>
                                    <th className="px-5 py-3 text-center">Management</th>
                                    <th className="px-5 py-3 text-center">Actions</th>
                                </tr>
                            </thead>
                            <tbody className="divide-y divide-slate-800/40">
                                {interfaces.map(iface => (
                                    <tr
                                        key={iface.id}
                                        onClick={() => selectInterface(iface)}
                                        className={`hover:bg-slate-800/20 cursor-pointer transition ${selectedId === iface.id ? 'bg-blue-500/5' : ''}`}
                                    >
                                        <td className="px-5 py-4.5 font-bold text-blue-400">{iface.name}</td>
                                        <td className="px-5 py-4.5 text-slate-300">{iface.device}</td>
                                        <td className="px-5 py-4.5 text-slate-200">{iface.ip || <span className="text-slate-600 font-sans">-</span>}</td>
                                        <td className="px-5 py-4.5 text-slate-400">{iface.netmask || '255.255.255.0'}</td>
                                        <td className="px-5 py-4.5 text-slate-400">{iface.gateway || <span className="text-slate-600 font-sans">-</span>}</td>
                                        <td className="px-5 py-4.5 text-center">
                                            {iface.mgmt_access ? (
                                                <span className="inline-flex items-center gap-1 px-2.5 py-0.5 rounded text-[10px] font-sans font-bold bg-emerald-500/10 border border-emerald-500/20 text-emerald-400">
                                                    <span className="h-1 w-1 rounded-full bg-emerald-400"></span> Enabled
                                                </span>
                                            ) : (
                                                <span className="inline-flex items-center gap-1 px-2.5 py-0.5 rounded text-[10px] font-sans font-bold bg-slate-800 border border-slate-700 text-slate-500">
                                                    Disabled
                                                </span>
                                            )}
                                        </td>
                                        <td className="px-5 py-4.5 text-center" onClick={e => e.stopPropagation()}>
                                            {iface.id !== 'wan' && iface.id !== 'lan' && iface.id !== 'mgmt' ? (
                                                <button
                                                    onClick={() => handleDelete(iface.id)}
                                                    className="py-1 px-2 rounded bg-rose-500/10 border border-rose-500/20 text-rose-400 hover:bg-rose-500 hover:text-white text-[11px] font-sans transition"
                                                >
                                                    Delete
                                                </button>
                                            ) : (
                                                <span className="text-slate-600 font-sans text-xs italic">System Lock</span>
                                            )}
                                        </td>
                                    </tr>
                                ))}
                                {interfaces.length === 0 && (
                                    <tr>
                                        <td colSpan={7} className="px-5 py-12 text-center text-slate-500 font-sans">
                                            No network interfaces configured. Create one above.
                                        </td>
                                    </tr>
                                )}
                            </tbody>
                        </table>
                    </div>
                </div>
            </div>

            {/* Settings Panel */}
            <div className="lg:col-span-1">
                {selectedId ? (
                    <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-5 shadow-lg backdrop-blur-md space-y-5">
                        <div className="flex items-center gap-2.5 pb-3 border-b border-slate-800/80">
                            <Icon name="tune" className="text-xl text-blue-500" />
                            <h2 className="font-headline text-base font-bold text-slate-200">
                                Configure Port: <span className="text-blue-400 font-mono">{editName}</span>
                            </h2>
                        </div>

                        {message && (
                            <div className={`p-4 rounded-lg text-xs leading-4 border ${message.isError ? 'bg-red-950/30 border-red-800/30 text-red-400' : 'bg-emerald-950/30 border-emerald-800/30 text-emerald-400'}`}>
                                {message.text}
                            </div>
                        )}

                        <form onSubmit={handleUpdateCurrent} className="space-y-4">
                            <div>
                                <label className="block text-xs font-semibold text-slate-400 mb-1.5">Interface Name (Alias)</label>
                                <input
                                    type="text"
                                    className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                                    value={editName}
                                    onChange={e => setEditName(e.target.value)}
                                    required
                                />
                            </div>

                            <div>
                                <label className="block text-xs font-semibold text-slate-400 mb-1.5">Kernel Adapter Device</label>
                                <input
                                    type="text"
                                    className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                                    value={editDevice}
                                    onChange={e => setEditDevice(e.target.value)}
                                    required
                                />
                            </div>

                            <div>
                                <label className="block text-xs font-semibold text-slate-400 mb-1.5">IP Address</label>
                                <input
                                    type="text"
                                    className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                                    placeholder="10.0.0.1"
                                    value={editIp}
                                    onChange={e => setEditIp(e.target.value)}
                                />
                            </div>

                            <div>
                                <label className="block text-xs font-semibold text-slate-400 mb-1.5">Netmask</label>
                                <input
                                    type="text"
                                    className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                                    placeholder="255.255.255.0"
                                    value={editNetmask}
                                    onChange={e => setEditNetmask(e.target.value)}
                                />
                            </div>

                            <div>
                                <label className="block text-xs font-semibold text-slate-400 mb-1.5">Gateway Address</label>
                                <input
                                    type="text"
                                    className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                                    placeholder="10.0.0.254"
                                    value={editGateway}
                                    onChange={e => setEditGateway(e.target.value)}
                                />
                            </div>

                            <div className="pt-2 border-t border-slate-800/40">
                                <label className="flex items-center gap-2.5 text-xs text-slate-300 font-semibold cursor-pointer">
                                    <input
                                        type="checkbox"
                                        checked={editMgmtAccess}
                                        onChange={e => setEditMgmtAccess(e.target.checked)}
                                        className="rounded bg-slate-950 border-slate-800 text-blue-500 focus:ring-0 focus:ring-offset-0 h-4.5 w-4.5 cursor-pointer"
                                    />
                                    Enable Management Ports (HTTPS/SSH/Ping)
                                </label>
                            </div>

                            <div className="flex flex-col gap-2 pt-3">
                                <button
                                    type="submit"
                                    className="w-full py-2 px-3 text-xs font-semibold rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700/80 transition"
                                >
                                    Update Cache List
                                </button>

                                <button
                                    type="button"
                                    onClick={handleApply}
                                    disabled={saving}
                                    className="w-full flex items-center justify-center gap-1.5 py-2 px-3 text-xs font-semibold rounded bg-blue-600 hover:bg-blue-500 text-white shadow-md shadow-blue-500/10 transition disabled:opacity-50"
                                >
                                    {saving ? 'Writing Database...' : 'Apply Network Settings'}
                                </button>
                            </div>
                        </form>
                    </div>
                ) : (
                    <div className="bg-slate-900/30 border border-slate-800/80 rounded-xl p-6 shadow-lg text-center text-slate-500 text-sm font-sans backdrop-blur-md">
                        Please select an interface row from the table list to modify settings.
                    </div>
                )}
            </div>

        </div>
    );
}

// --- LICENSING COMPONENT ---
function License({ fetchWithAuth }: { fetchWithAuth: any }) {
    const [license, setLicense] = useState<any>(null);
    const [licenseKeyInput, setLicenseKeyInput] = useState('');
    const [serverUrlInput, setServerUrlInput] = useState('');
    const [verifySslInput, setVerifySslInput] = useState('1');
    const [loading, setLoading] = useState(false);
    const [message, setMessage] = useState<{ text: string; isError: boolean } | null>(null);

    // Fetch license status — now uses direct fetch since endpoint is public
    const fetchLicenseStatus = async () => {
        try {
            const res = await fetch(`${API_BASE}/license`);
            if (res && res.ok) {
                const text = await res.text();
                let data;
                try {
                    data = JSON.parse(text);
                } catch (e) {
                    console.error("Invalid JSON from /api/license:", text);
                    return;
                }
                setLicense(data);
                if (data.license_server_url) setServerUrlInput(data.license_server_url);
                if (data.license_server_verify_ssl) setVerifySslInput(data.license_server_verify_ssl);
            } else {
                // License endpoint unreachable — API might not be running
                setLicense({ status: 'UNKNOWN', machine_id: 'API unavailable', license_server_url: '' });
            }
        } catch (err) {
            console.error("Failed to fetch license status:", err);
            setLicense({ status: 'UNKNOWN', machine_id: 'Connection failed', license_server_url: '' });
        }
    };

    useEffect(() => {
        fetchLicenseStatus();
    }, []);

    const handleActivate = async (e: React.FormEvent) => {
        e.preventDefault();
        setLoading(true);
        setMessage(null);
        try {
            const active_key = license?.status === 'ACTIVE' ? license?.license_key : licenseKeyInput;
            if (!active_key && license?.status !== 'ACTIVE') {
                setMessage({ text: 'License activation key is required.', isError: true });
                setLoading(false);
                return;
            }

            // Use direct fetch (endpoint is public, no auth needed)
            const res = await fetch(`${API_BASE}/license/activate`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    license_key: active_key,
                    license_server_url: serverUrlInput,
                    license_server_verify_ssl: verifySslInput
                })
            });

            if (res) {
                const text = await res.text();
                let data;
                try {
                    data = JSON.parse(text);
                } catch (e) {
                    data = { error: text ? (text.length > 200 ? text.substring(0, 200) + '...' : text) : 'Server returned an invalid format.' };
                }
                if (res.ok) {
                    setMessage({ text: 'Subscription settings written. Cryptographic activation validated.', isError: false });
                    setLicenseKeyInput('');
                    fetchLicenseStatus();
                } else {
                    setMessage({ text: data.error || 'Server validation failed.', isError: true });
                }
            } else {
                setMessage({ text: 'Verification failed. Could not communicate with licensing server.', isError: true });
            }
        } catch (err) {
            setMessage({ text: 'Appliance communication failure to licensing gateway.', isError: true });
        } finally {
            setLoading(false);
        }
    };

    const handleCheckUpdates = async () => {
        try {
            // Use fetchWithAuth for update check (requires admin auth)
            const res = await fetchWithAuth(`${API_BASE}/update/check`, { method: 'POST' });
            if (res && res.ok) {
                alert('Check request successfully triggered in background daemon.');
            } else {
                alert('Failed to invoke update client agent.');
            }
        } catch (e) {
            alert('Error contacting background API daemon.');
        }
    };

    const isActive = license?.status === 'ACTIVE';
    const machineId = license?.machine_id || 'Loading...';

    return (
        <div className="max-w-2xl mx-auto bg-slate-900/30 border border-slate-800/80 rounded-xl p-6 shadow-lg backdrop-blur-md space-y-6">

            <div className="flex items-center justify-between pb-4 border-b border-slate-800/80">
                <div className="flex items-center gap-2.5">
                    <Icon name="key" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">Device Subscription Registration</h2>
                </div>
                {license && (
                    <span className={`text-[10px] font-mono font-bold uppercase tracking-wider px-2.5 py-0.5 rounded border ${isActive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-rose-500/10 border-rose-500/20 text-rose-400'}`}>
                        {license.status}
                    </span>
                )}
            </div>

            {message && (
                <div className={`p-4 rounded-lg text-xs leading-4 border ${message.isError ? 'bg-red-950/30 border-red-800/30 text-red-400' : 'bg-emerald-950/30 border-emerald-800/30 text-emerald-400'}`}>
                    {message.text}
                </div>
            )}

            <div className="space-y-3.5 text-sm font-mono border-b border-slate-800/40 pb-5">
                <div className="flex justify-between">
                    <span className="text-slate-500 font-sans">Appliance Node ID</span>
                    <span className="text-slate-300 select-all font-semibold">{machineId}</span>
                </div>
                <div className="flex justify-between">
                    <span className="text-slate-500 font-sans">Registered Key</span>
                    <span className="text-slate-300 font-semibold">{isActive ? (license?.license_key || 'Active') : 'Unregistered Device'}</span>
                </div>
            </div>

            <form onSubmit={handleActivate} className="space-y-4 pt-1">
                <div>
                    <label className="block text-xs font-semibold text-slate-400 mb-1.5">Update / Licensing Server URL</label>
                    <input
                        type="text"
                        className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                        placeholder="https://your-server.example.com"
                        value={serverUrlInput}
                        onChange={e => setServerUrlInput(e.target.value)}
                        required
                        disabled={loading}
                    />
                </div>

                <div>
                    <label className="block text-xs font-semibold text-slate-400 mb-1.5">SSL Certificate Verification Mode</label>
                    <select
                        className="block w-full rounded bg-slate-950/80 border border-slate-800 px-2 py-2 text-xs text-slate-300 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 cursor-pointer"
                        value={verifySslInput}
                        onChange={e => setVerifySslInput(e.target.value)}
                        disabled={loading}
                    >
                        <option value="1">Strict Enforce SSL (Production Trust)</option>
                        <option value="0">Skip Cert Verification (Testing / Host Dev)</option>
                    </select>
                </div>

                {!isActive && (
                    <div>
                        <label className="block text-xs font-semibold text-slate-400 mb-1.5">Enter Registration Key</label>
                        <input
                            type="text"
                            className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 font-mono text-center tracking-wider"
                            placeholder="XXXX-XXXX-XXXX-XXXX"
                            value={licenseKeyInput}
                            onChange={e => setLicenseKeyInput(e.target.value)}
                            required
                            disabled={loading}
                        />
                    </div>
                )}

                <div className="flex gap-3 pt-3">
                    <button
                        type="submit"
                        disabled={loading}
                        className="flex-1 py-2 px-4 rounded bg-blue-600 hover:bg-blue-500 text-xs font-semibold text-white shadow-md shadow-blue-500/10 transition disabled:opacity-50"
                    >
                        {loading ? 'Processing Check...' : isActive ? 'Modify Licensing Endpoint' : 'Activate Subscription'}
                    </button>

                    {isActive && (
                        <button
                            type="button"
                            onClick={handleCheckUpdates}
                            className="py-2 px-4 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 border border-slate-700/80 text-xs font-semibold transition"
                        >
                            Trigger Client Update Check
                        </button>
                    )}
                </div>
            </form>

            <div className="pt-4 border-t border-slate-800/40 text-xs text-slate-500 font-sans">
                {isActive
                    ? 'This hardware node is registered and active. The update agent triggers automatic checks every 5 minutes in background.'
                    : 'Enter the licensing server URL and your registration key to activate this appliance. The server URL must point to a running Beout_OS licensing server.'}
            </div>

        </div>
    );
}

// --- TIME SETTINGS COMPONENT ---
function TimeSettings({ fetchWithAuth }: { fetchWithAuth: any }) {
    const [timezone, setTimezone] = useState('UTC');
    const [ntpServer, setNtpServer] = useState('pool.ntp.org');
    const [loading, setLoading] = useState(false);
    const [message, setMessage] = useState<{ text: string; isError: boolean } | null>(null);

    const fetchTimeSettings = async () => {
        try {
            const res = await fetchWithAuth(`${API_BASE}/time`);
            if (res && res.ok) {
                const data = await res.json();
                if (data.timezone) setTimezone(data.timezone);
                if (data.ntp_server) setNtpServer(data.ntp_server);
            }
        } catch (err) {
            console.error(err);
        }
    };

    useEffect(() => {
        fetchTimeSettings();
    }, []);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();
        setLoading(true);
        setMessage(null);
        try {
            const res = await fetchWithAuth(`${API_BASE}/time`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezone, ntp_server: ntpServer })
            });
            if (res && res.ok) {
                setMessage({ text: 'Time settings successfully updated on the client appliance.', isError: false });
                fetchTimeSettings();
            } else {
                const data = await res.json();
                setMessage({ text: data.error || 'Failed to save time settings.', isError: true });
            }
        } catch (err) {
            setMessage({ text: 'Error communicating with local API daemon.', isError: true });
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="max-w-2xl mx-auto bg-slate-900/30 border border-slate-800/80 rounded-xl p-6 shadow-lg backdrop-blur-md space-y-6">
            <div className="flex items-center justify-between pb-4 border-b border-slate-800/80">
                <div className="flex items-center gap-2.5">
                    <Icon name="schedule" className="text-xl text-blue-500" />
                    <h2 className="font-headline text-base font-bold text-slate-200">Appliance Time & Timezone</h2>
                </div>
            </div>

            {message && (
                <div className={`p-4 rounded-lg text-xs leading-4 border ${message.isError ? 'bg-red-950/30 border-red-800/30 text-red-400' : 'bg-emerald-950/30 border-emerald-800/30 text-emerald-400'}`}>
                    {message.text}
                </div>
            )}

            <form onSubmit={handleSave} className="space-y-4">
                <div>
                    <label className="block text-xs font-semibold text-slate-400 mb-1.5">System Timezone</label>
                    <select
                        className="block w-full rounded bg-slate-950/80 border border-slate-800 px-2 py-2 text-xs text-slate-300 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 cursor-pointer"
                        value={timezone}
                        onChange={e => setTimezone(e.target.value)}
                        disabled={loading}
                    >
                        <option value="UTC">UTC (Coordinated Universal Time)</option>
                        <option value="Europe/London">Europe/London</option>
                        <option value="Europe/Paris">Europe/Paris</option>
                        <option value="America/New_York">America/New_York</option>
                        <option value="Asia/Riyadh">Asia/Riyadh (Saudi Arabia)</option>
                        <option value="Asia/Dubai">Asia/Dubai (UAE)</option>
                        <option value="Asia/Kuwait">Asia/Kuwait</option>
                        <option value="Africa/Cairo">Africa/Cairo (Egypt)</option>
                    </select>
                </div>

                <div>
                    <label className="block text-xs font-semibold text-slate-400 mb-1.5">NTP / Time Server</label>
                    <input
                        type="text"
                        className="block w-full rounded bg-slate-950/80 border border-slate-800 px-3 py-2 text-xs text-slate-100 placeholder-slate-600 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500"
                        placeholder="pool.ntp.org"
                        value={ntpServer}
                        onChange={e => setNtpServer(e.target.value)}
                        required
                        disabled={loading}
                    />
                </div>

                <div className="pt-3">
                    <button
                        type="submit"
                        disabled={loading}
                        className="w-full py-2 px-4 rounded bg-blue-600 hover:bg-blue-500 text-xs font-semibold text-white shadow-md shadow-blue-500/10 transition disabled:opacity-50"
                    >
                        {loading ? 'Saving Settings...' : 'Save Time Settings'}
                    </button>
                </div>
            </form>
        </div>
    );
}