import React, { useState, useEffect } from 'react';
import { Shield, Server, Activity, Key, LogOut, LayoutDashboard } from 'lucide-react';

const API_BASE = '/api';

export default function App() {
  const [token, setToken] = useState<string | null>(localStorage.getItem('auth_token'));
  const [activeTab, setActiveTab] = useState('dashboard');
  
  if (!token) {
    return <Login setToken={setToken} />;
  }

  return (
    <div className="app-container">
      <Sidebar activeTab={activeTab} setActiveTab={setActiveTab} setToken={setToken} />
      <main className="main-content">
        <div className="animate-fade-in">
          {activeTab === 'dashboard' && <Dashboard />}
          {activeTab === 'interfaces' && <Interfaces token={token} />}
          {activeTab === 'license' && <License token={token} />}
        </div>
      </main>
    </div>
  );
}

function Login({ setToken }: { setToken: (t: string) => void }) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();
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
        setError(data.error || 'Login failed');
      }
    } catch (err) {
      setError('Network error');
    }
  };

  return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '100vh' }}>
      <div className="glass-panel" style={{ width: '400px' }}>
        <div style={{ textAlign: 'center', marginBottom: '2rem' }}>
          <Shield size={48} color="var(--accent-primary)" style={{ marginBottom: '1rem' }} />
          <h1 style={{ fontSize: '1.5rem', fontWeight: 700 }}>Beout_OS</h1>
          <p style={{ color: 'var(--text-secondary)', fontSize: '0.875rem' }}>Security Appliance Management</p>
        </div>
        
        {error && (
          <div style={{ background: 'rgba(239, 68, 68, 0.1)', color: 'var(--danger)', padding: '12px', borderRadius: '8px', marginBottom: '16px', fontSize: '0.875rem', border: '1px solid rgba(239, 68, 68, 0.3)' }}>
            {error}
          </div>
        )}

        <form onSubmit={handleLogin}>
          <div className="input-group">
            <label className="input-label">Username</label>
            <input type="text" className="input-field" value={username} onChange={e => setUsername(e.target.value)} required />
          </div>
          <div className="input-group">
            <label className="input-label">Password</label>
            <input type="password" className="input-field" value={password} onChange={e => setPassword(e.target.value)} required />
          </div>
          <button type="submit" className="btn btn-primary" style={{ width: '100%', marginTop: '1rem' }}>
            Authenticate
          </button>
        </form>
      </div>
    </div>
  );
}

function Sidebar({ activeTab, setActiveTab, setToken }: any) {
  const handleLogout = () => {
    localStorage.removeItem('auth_token');
    setToken(null);
  };

  return (
    <aside className="sidebar">
      <div className="sidebar-logo">
        <Shield size={28} color="var(--accent-primary)" />
        Beout_OS
      </div>
      <nav style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <div className={`nav-item ${activeTab === 'dashboard' ? 'active' : ''}`} onClick={() => setActiveTab('dashboard')}>
          <LayoutDashboard size={20} /> System Status
        </div>
        <div className={`nav-item ${activeTab === 'interfaces' ? 'active' : ''}`} onClick={() => setActiveTab('interfaces')}>
          <Server size={20} /> Interfaces
        </div>
        <div className={`nav-item ${activeTab === 'license' ? 'active' : ''}`} onClick={() => setActiveTab('license')}>
          <Key size={20} /> Licensing
        </div>
      </nav>
      <div style={{ padding: '24px' }}>
        <button onClick={handleLogout} className="btn" style={{ width: '100%', background: 'rgba(255,255,255,0.1)', color: 'white' }}>
          <LogOut size={16} style={{ marginRight: '8px' }} /> Disconnect
        </button>
      </div>
    </aside>
  );
}

function Dashboard() {
  const [health, setHealth] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/health`)
      .then(r => r.json())
      .then(setHealth)
      .catch(console.error);
  }, []);

  return (
    <div>
      <header className="page-header">
        <h1 className="page-title">System Status</h1>
      </header>
      
      <div className="grid-cards">
        <div className="glass-panel animate-fade-in delay-1">
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
            <Activity color="var(--accent-primary)" />
            <h2 style={{ fontSize: '1.25rem' }}>Engine Health</h2>
          </div>
          <div className="data-row">
            <span className="data-label">Status</span>
            <span className="status-badge status-active">{health?.status || 'Loading...'}</span>
          </div>
          <div className="data-row">
            <span className="data-label">OS Version</span>
            <span className="data-value">{health?.version || '--'}</span>
          </div>
          <div className="data-row">
            <span className="data-label">Uptime</span>
            <span className="data-value">99.9% (Demo)</span>
          </div>
        </div>
      </div>
    </div>
  );
}

function Interfaces({ token }: { token: string }) {
  const [config, setConfig] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/config`, {
      headers: { 'Authorization': `Bearer ${token}` }
    })
      .then(r => r.json())
      .then(setConfig)
      .catch(console.error);
  }, [token]);

  return (
    <div>
      <header className="page-header">
        <h1 className="page-title">Network Interfaces</h1>
      </header>
      
      <div className="grid-cards">
        {['WAN', 'LAN', 'MGMT'].map((iface, i) => (
          <div key={iface} className={`glass-panel animate-fade-in delay-${i + 1}`}>
            <h2 style={{ fontSize: '1.25rem', marginBottom: '16px' }}>{iface} Port</h2>
            <div className="data-row">
              <span className="data-label">Interface Device</span>
              <span className="data-value">{config ? config[`${iface.toLowerCase()}_interface`] || 'Unassigned' : 'Loading...'}</span>
            </div>
            <div className="data-row">
              <span className="data-label">IP Address</span>
              <span className="data-value">{config ? config[`${iface.toLowerCase()}_ip`] || 'Unconfigured' : 'Loading...'}</span>
            </div>
            <div className="data-row">
              <span className="data-label">Netmask</span>
              <span className="data-value">{config ? config[`${iface.toLowerCase()}_netmask`] || 'Unconfigured' : 'Loading...'}</span>
            </div>
            {(iface === 'WAN' || iface === 'MGMT') && (
              <div className="data-row">
                <span className="data-label">Gateway</span>
                <span className="data-value">{config ? config[`${iface.toLowerCase()}_gateway`] || 'None' : 'Loading...'}</span>
              </div>
            )}
            <div className="data-row">
              <span className="data-label">Link Status</span>
              <span className="status-badge status-active">UP</span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

function License({ token }: { token: string }) {
  const [license, setLicense] = useState<any>(null);
  const [licenseKeyInput, setLicenseKeyInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ text: string; isError: boolean } | null>(null);

  const fetchLicenseStatus = () => {
    fetch(`${API_BASE}/license`, {
      headers: { 'Authorization': `Bearer ${token}` }
    })
      .then(r => r.json())
      .then(setLicense)
      .catch(console.error);
  };

  useEffect(() => {
    fetchLicenseStatus();
  }, [token]);

  const handleActivate = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);
    setMessage(null);
    try {
      const res = await fetch(`${API_BASE}/license/activate`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${token}`
        },
        body: JSON.stringify({ license_key: licenseKeyInput })
      });
      const data = await res.json();
      if (res.ok) {
        setMessage({ text: 'Appliance activated successfully!', isError: false });
        setLicenseKeyInput('');
        fetchLicenseStatus();
      } else {
        setMessage({ text: data.error || 'Activation failed', isError: true });
      }
    } catch (err) {
      setMessage({ text: 'Network connection failed', isError: true });
    } finally {
      setLoading(false);
    }
  };

  const isActive = license?.status === 'ACTIVE';

  return (
    <div>
      <header className="page-header">
        <h1 className="page-title">Appliance Licensing</h1>
      </header>
      
      <div className="glass-panel animate-fade-in delay-1" style={{ maxWidth: '600px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '24px' }}>
          <h2 style={{ fontSize: '1.25rem' }}>Activation Lock</h2>
          {license && (
            <span className={`status-badge ${isActive ? 'status-active' : 'status-inactive'}`}>
              {license.status}
            </span>
          )}
        </div>
        
        {message && (
          <div style={{
            background: message.isError ? 'rgba(239, 68, 68, 0.1)' : 'rgba(48, 209, 88, 0.1)',
            color: message.isError ? 'var(--danger)' : 'var(--success)',
            padding: '12px',
            borderRadius: '8px',
            marginBottom: '16px',
            fontSize: '0.875rem',
            border: `1px solid ${message.isError ? 'rgba(239, 68, 68, 0.3)' : 'rgba(48, 209, 88, 0.3)'}`
          }}>
            {message.text}
          </div>
        )}

        <div className="data-row">
          <span className="data-label">Appliance Machine ID</span>
          <span className="data-value" style={{ fontFamily: 'monospace', background: 'rgba(255,255,255,0.05)', padding: '2px 6px', borderRadius: '4px', fontSize: '0.8rem', userSelect: 'all' }}>
            {license?.machine_id || 'Loading...'}
          </span>
        </div>

        <div className="data-row">
          <span className="data-label">License Key</span>
          <span className="data-value" style={{ fontFamily: 'monospace' }}>
            {isActive ? license?.license_key : 'Unlicensed'}
          </span>
        </div>
        
        {!isActive && (
          <form onSubmit={handleActivate} style={{ marginTop: '24px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div className="input-group">
              <label className="input-label">Enter License Key</label>
              <input 
                type="text" 
                className="input-field" 
                placeholder="XXXX-XXXX-XXXX-XXXX" 
                value={licenseKeyInput} 
                onChange={e => setLicenseKeyInput(e.target.value)} 
                required 
                disabled={loading}
              />
            </div>
            <button type="submit" className="btn btn-primary" style={{ width: '100%' }} disabled={loading}>
              {loading ? 'Activating Appliance...' : 'Activate Appliance'}
            </button>
          </form>
        )}

        <div style={{ marginTop: '24px', paddingTop: '24px', borderTop: '1px solid var(--border-color)' }}>
          <p style={{ color: 'var(--text-secondary)', fontSize: '0.875rem' }}>
            {isActive 
              ? 'This appliance is fully activated and cryptographically locked to this hardware instance.'
              : 'This appliance requires activation. Please generate a license key on the Beout_OS Main Server and enter it above.'}
          </p>
        </div>
      </div>
    </div>
  );
}
