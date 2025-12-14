import { createContext, useContext, useState, useEffect, type ReactNode } from 'react';
import { isAuthenticated, logout as apiLogout, loginWithOAuthCode, getCurrentSession } from './api';

interface AuthContextType {
    isLoggedIn: boolean;
    email: string | null;
    loginWithCode: (code: string) => Promise<void>;
    logout: () => Promise<void>;
    loading: boolean;
}

const AuthContext = createContext<AuthContextType | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
    const [isLoggedIn, setIsLoggedIn] = useState(false);
    const [email, setEmail] = useState<string | null>(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        // Check if already authenticated
        const checkAuth = async () => {
            if (isAuthenticated()) {
                try {
                    const session = await getCurrentSession();
                    setIsLoggedIn(true);
                    setEmail(session.email);
                } catch {
                    // Token invalid, clear it
                    setIsLoggedIn(false);
                    setEmail(null);
                }
            }
            setLoading(false);
        };
        checkAuth();
    }, []);

    const loginWithCode = async (code: string) => {
        const session = await loginWithOAuthCode(code);
        setIsLoggedIn(true);
        setEmail(session.email);
    };

    const logout = async () => {
        await apiLogout();
        setIsLoggedIn(false);
        setEmail(null);
    };

    return (
        <AuthContext.Provider value={{ isLoggedIn, email, loginWithCode, logout, loading }}>
            {children}
        </AuthContext.Provider>
    );
}

export function useAuth() {
    const context = useContext(AuthContext);
    if (!context) {
        throw new Error('useAuth must be used within AuthProvider');
    }
    return context;
}

