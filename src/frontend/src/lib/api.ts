/**
 * API Client for Kherashanu Backend
 */

const API_BASE = import.meta.env.VITE_API_URL || '';

// Types
export interface BlogPost {
    id: number;
    title: string;
    url_slug: string;
    description: string;
    summary: string;
    publish_date: number;
    category: string;
    tags: string;
    content: string;
    status: number;  // 0 = DRAFT, 1 = PUBLISHED
}

export interface Session {
    token: string;
    email: string;
    expires_at: number;
}

export interface ApiError {
    error: string;
}

export interface Stats {
    total_posts: number;
    published: number;
    drafts: number;
    visits: number;
}

// Storage keys
const TOKEN_KEY = 'kherashanu_token';
const EMAIL_KEY = 'kherashanu_email';

// Token management
export function getToken(): string | null {
    return localStorage.getItem(TOKEN_KEY);
}

export function setToken(token: string, email: string): void {
    localStorage.setItem(TOKEN_KEY, token);
    localStorage.setItem(EMAIL_KEY, email);
}

export function clearToken(): void {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(EMAIL_KEY);
}

export function getEmail(): string | null {
    return localStorage.getItem(EMAIL_KEY);
}

export function isAuthenticated(): boolean {
    return !!getToken();
}

// API helper
async function apiFetch<T>(
    endpoint: string,
    options: RequestInit = {}
): Promise<T> {
    const token = getToken();
    const headers: HeadersInit = {
        'Content-Type': 'application/json',
        ...options.headers,
    };

    if (token) {
        (headers as Record<string, string>)['Authorization'] = `Bearer ${token}`;
    }

    const response = await fetch(`${API_BASE}${endpoint}`, {
        ...options,
        headers,
    });

    const data = await response.json();

    if (!response.ok) {
        throw new Error((data as ApiError).error || 'Request failed');
    }

    return data as T;
}

// Public API
export async function getPublishedBlogs(): Promise<BlogPost[]> {
    return apiFetch<BlogPost[]>('/api/blogs');
}

export async function getBlogBySlug(slug: string): Promise<BlogPost> {
    return apiFetch<BlogPost>(`/api/blog/${slug}`);
}

export async function getLatestBlog(): Promise<BlogPost> {
    return apiFetch<BlogPost>('/api/blogs/latest');
}

// Auth API
export async function loginWithGoogle(email: string): Promise<Session> {
    const session = await apiFetch<Session>('/api/auth/google', {
        method: 'POST',
        body: JSON.stringify({ email }),
    });
    setToken(session.token, session.email);
    return session;
}

export async function loginWithOAuthCode(code: string): Promise<Session> {
    const redirect_uri = `${window.location.origin}/auth`;
    const session = await apiFetch<Session>('/api/auth/google', {
        method: 'POST',
        body: JSON.stringify({ code, redirect_uri }),
    });
    setToken(session.token, session.email);
    return session;
}

export async function getCurrentSession(): Promise<{ email: string; expires_at: number }> {
    return apiFetch('/api/auth/me');
}

export async function logout(): Promise<void> {
    try {
        await apiFetch('/api/auth/logout', { method: 'POST' });
    } finally {
        clearToken();
    }
}

// Admin API
export async function getAdminBlogs(): Promise<BlogPost[]> {
    return apiFetch<BlogPost[]>('/api/admin/blogs');
}

export async function createBlog(blog: Partial<BlogPost>): Promise<BlogPost> {
    return apiFetch<BlogPost>('/api/admin/blog', {
        method: 'POST',
        body: JSON.stringify(blog),
    });
}

export async function updateBlog(slug: string, blog: Partial<BlogPost>): Promise<BlogPost> {
    return apiFetch<BlogPost>(`/api/admin/blog/${slug}`, {
        method: 'PUT',
        body: JSON.stringify(blog),
    });
}

export async function deleteBlog(slug: string): Promise<void> {
    await apiFetch(`/api/admin/blog/${slug}`, { method: 'DELETE' });
}

export async function getStats(): Promise<Stats> {
    return apiFetch<Stats>('/api/admin/stats');
}
