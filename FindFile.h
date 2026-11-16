#pragma once

class CFindFile
{
public:

	CFindFile(void)
	{
		m_hFindFile = INVALID_HANDLE_VALUE;
	}

	virtual ~CFindFile(void)
	{
	}

	BOOL StartFind( const char * pszPath, const char * pszFindName, BOOL bFindSubFolder = FALSE )
	{
		WIN32_FIND_DATA	wfd;
		CHAR	szCombinedPath[1024];

		_makepath( m_szFileName, NULL, pszPath, pszFindName, NULL );
		m_hFindFile = FindFirstFile( m_szFileName, &wfd );
		if( m_hFindFile != INVALID_HANDLE_VALUE )
		{
			do {

				if( (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 )
				{
					if( bFindSubFolder )
					{
						_makepath( szCombinedPath, NULL, pszPath, wfd.cFileName, NULL );
						StartFind( szCombinedPath, pszFindName, TRUE );
					}
				}
				else
				{
					_makepath( m_szFileName, NULL, pszPath, wfd.cFileName, NULL );
					OnFoundFile( m_szFileName );
				}
			} while(FindNextFile( m_hFindFile, &wfd));
			FindClose( m_hFindFile );
		}
		return TRUE;
	}
protected:
	virtual VOID OnFoundFile( const char * pszFilename ){}
private:
    HANDLE	m_hFindFile;
	char	m_szFileName[1024];
};
