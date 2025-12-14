import { useEffect } from 'react';

export default function usePrefetch(url: string) {
    useEffect(() => {
        const link = document.createElement('link');
        link.rel = 'prefetch';
        link.href = url;
        document.head.appendChild(link);

        return () => {
            document.head.removeChild(link);
        };
    }, [url]);
}
